#ifndef TESTHTTPSERVER_H
#define TESTHTTPSERVER_H

#include <QHash>
#include <QList>
#include <QTcpServer>
#include <QTcpSocket>

// Minimal HTTP/1.1 server for the updater tests: GET only, one request per
// connection, with the Range / If-Range behaviour of a release CDN, plus the
// broken networks the fetcher must survive (cut, hang).
class TestHttpServer : public QTcpServer
{
public:
    struct Route {
        QByteArray body{};
        QByteArray etag{};      // sent on 200/206; empty = no validator
        int status = 200;       // != 200: that status, empty body
        qint64 cutAfter = -1;   // >= 0: the body stops after N bytes, then the connection closes
        bool hang = false;      // the connection stays open instead, sending nothing more
    };
    struct Request {
        QByteArray path;
        QByteArray range;
        QByteArray ifRange;
        int status = 0;         // the answer it got
    };

    QHash<QByteArray, Route> routes;
    QList<Request> requests;

    TestHttpServer()
    {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    QString url(const QByteArray &path) const
    {
        return QStringLiteral("http://127.0.0.1:%1%2").arg(serverPort()).arg(QString::fromLatin1(path));
    }

    int count(const QByteArray &path) const
    {
        int n = 0;
        for (const Request &r : requests)
            n += r.path == path;
        return n;
    }

    Request last(const QByteArray &path) const
    {
        for (auto it = requests.crbegin(); it != requests.crend(); ++it)
            if (it->path == path)
                return *it;
        return {};
    }

private:
    QHash<QTcpSocket *, QByteArray> m_buffers;

    void onReadyRead(QTcpSocket *socket)
    {
        QByteArray &buffer = m_buffers[socket];
        buffer += socket->readAll();
        const qsizetype end = buffer.indexOf("\r\n\r\n");
        if (end < 0)
            return; // head not complete yet
        const QList<QByteArray> lines = buffer.left(end).split('\n');
        m_buffers.remove(socket);

        Request request;
        request.path = lines.value(0).trimmed().split(' ').value(1);
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const QByteArray line = lines.at(i).trimmed();
            const qsizetype colon = line.indexOf(':');
            if (colon < 0)
                continue;
            const QByteArray name = line.left(colon).trimmed().toLower();
            const QByteArray value = line.mid(colon + 1).trimmed();
            if (name == "range")
                request.range = value;
            else if (name == "if-range")
                request.ifRange = value;
        }
        requests.append(request);
        respond(socket, requests.last());
    }

    void respond(QTcpSocket *socket, Request &request)
    {
        const auto it = routes.constFind(request.path);
        if (it == routes.cend()) {
            send(socket, request, 404, {}, {});
            return;
        }
        const Route &route = *it;
        if (route.status != 200) {
            send(socket, request, route.status, {}, {});
            return;
        }

        QList<QByteArray> headers;
        if (!route.etag.isEmpty())
            headers << "ETag: " + route.etag;
        const qint64 size = route.body.size();
        // A changed file (If-Range no longer matches) comes back whole
        const bool partial = request.range.startsWith("bytes=")
                && (request.ifRange.isEmpty() || request.ifRange == route.etag);
        qint64 start = 0;
        if (partial) {
            start = request.range.mid(6, request.range.indexOf('-') - 6).toLongLong();
            if (start >= size) {
                send(socket, request, 416, {"Content-Range: bytes */" + QByteArray::number(size)}, {});
                return;
            }
            headers << "Content-Range: bytes " + QByteArray::number(start) + '-'
                       + QByteArray::number(size - 1) + '/' + QByteArray::number(size);
        }
        const QByteArray body = route.body.mid(start);
        if (route.hang || route.cutAfter >= 0) {
            // Promises the whole body, sends part of it
            request.status = partial ? 206 : 200;
            writeHead(socket, request.status, headers, body.size());
            socket->write(body.left(qMax<qint64>(route.cutAfter, 0)));
            if (!route.hang)
                socket->disconnectFromHost(); // after the pending bytes
            return;
        }
        send(socket, request, partial ? 206 : 200, headers, body);
    }

    static void writeHead(QTcpSocket *socket, int status, const QList<QByteArray> &headers,
                          qint64 length)
    {
        // Clients read the code, not the phrase
        QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + " Status\r\n";
        for (const QByteArray &h : headers)
            head += h + "\r\n";
        head += "Content-Length: " + QByteArray::number(length) + "\r\nConnection: close\r\n\r\n";
        socket->write(head);
    }

    static void send(QTcpSocket *socket, Request &request, int status,
                     const QList<QByteArray> &headers, const QByteArray &body)
    {
        request.status = status;
        writeHead(socket, status, headers, body.size());
        socket->write(body);
        socket->disconnectFromHost();
    }
};

#endif // TESTHTTPSERVER_H
