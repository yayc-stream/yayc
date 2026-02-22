/*
Copyright (C) 2023- YAYC team <yaycteam@gmail.com>

This work is licensed under the terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/ or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

In addition to the above,
- The use of this work for training, fine-tuning, or otherwise feeding artificial intelligence systems is prohibited for both commercial and non-commercial use.
  This includes, but is not limited to, the ingestion of this work into large language models (LLMs), code generation models,
  Retrieval-Augmented Generation (RAG) systems, embedding databases, vector stores, or any other AI-assisted system.
- Any and all donation options in derivative work must be the same as in the original work.
- All use of this work outside of the above terms must be explicitly agreed upon in advance with the exclusive copyright owner(s).
- Any derivative work must retain the above copyright and acknowledge that any and all use of the derivative work outside the above terms
  must be explicitly agreed upon in advance with the exclusive copyright owner(s) of the original work.

*/

#ifndef NODIRSORTPROXYMODEL_H
#define NODIRSORTPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QFileSystemModel>
#include <QRegularExpression>

class NoDirSortProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

    QString m_searchTerm;
    bool m_searchInTitles{true};
    bool m_searchInChannelNames{true};

    bool m_searchInStarred{true};
    bool m_searchInUnstarred{true};
    bool m_searchInOpened{true};
    bool m_searchInUnopened{true};
    bool m_searchInWatched{true};
    bool m_searchInUnwatched{true};
    bool m_searchInSaved{true};
    bool m_searchInUnsaved{true};
    bool m_searchInShorts{true};
    QString m_workingDirRoot;

    Q_PROPERTY(QString searchTerm READ searchTerm WRITE setSearchTerm NOTIFY searchTermChanged)
    Q_PROPERTY(bool searchInTitles READ searchInTitles WRITE setSearchInTitles NOTIFY searchInTitlesChanged)
    Q_PROPERTY(bool searchInChannelNames READ searchInChannelNames WRITE setSearchInChannelNames NOTIFY searchInChannelNamesChanged)

    Q_PROPERTY(bool searchInStarred MEMBER m_searchInStarred NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInUnstarred MEMBER m_searchInUnstarred NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInOpened MEMBER m_searchInOpened NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInUnopened MEMBER m_searchInUnopened NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInWatched MEMBER m_searchInWatched NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInUnwatched MEMBER m_searchInUnwatched NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInSaved MEMBER m_searchInSaved NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInUnsaved MEMBER m_searchInUnsaved NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInShorts MEMBER m_searchInShorts NOTIFY searchParametersChanged)
    Q_PROPERTY(QString workingDirRoot MEMBER m_workingDirRoot NOTIFY searchParametersChanged)

public:
    NoDirSortProxyModel();
    ~NoDirSortProxyModel() override;

    QString searchTerm() const;
    void setSearchTerm(const QString &term);
    bool searchInTitles() const;
    void setSearchInTitles(bool enabled);
    bool searchInChannelNames() const;
    void setSearchInChannelNames(bool enabled);

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    void updateSearchTerm();

signals:
    void searchTermChanged();
    void searchInTitlesChanged();
    void searchInChannelNamesChanged();
    void searchParametersChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
};

#endif // NODIRSORTPROXYMODEL_H
