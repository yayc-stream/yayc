/*
Copyright (C) 2023- YAYC team <yaycteam@gmail.com>

This work is licensed under the terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/ or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

In addition to the above,
- The use of this work for training artificial intelligence is prohibited for both commercial and non-commercial use.
- Any and all donation options in derivative work must be the same as in the original work.
- All use of this work outside of the above terms must be explicitly agreed upon in advance with the exclusive copyright owner(s).
- Any derivative work must retain the above copyright and acknowledge that any and all use of the derivative work outside the above terms
  must be explicitly agreed upon in advance with the exclusive copyright owner(s) of the original work.

*/

#include "NoDirSortProxyModel.h"
#include "FileSystemModel.h"
#include "YaycUtilities.h"
#include "Platform.h"

#include <QFileInfo>

NoDirSortProxyModel::NoDirSortProxyModel() : QSortFilterProxyModel() {
    connect(this, &NoDirSortProxyModel::searchParametersChanged, [&]() {
        updateSearchTerm();
    });
}

NoDirSortProxyModel::~NoDirSortProxyModel() {}

QString NoDirSortProxyModel::searchTerm() const {
    return m_searchTerm;
}

void NoDirSortProxyModel::setSearchTerm(const QString &term) {
    if (term == m_searchTerm)
        return;

    m_searchTerm = term;
    updateSearchTerm();

    emit searchTermChanged();
}

bool NoDirSortProxyModel::searchInTitles() const {
    return m_searchInTitles;
}

void NoDirSortProxyModel::setSearchInTitles(bool enabled) {
    if (enabled == m_searchInTitles)
        return;

    m_searchInTitles = enabled;
    updateSearchTerm();
    emit searchInTitlesChanged();
}

bool NoDirSortProxyModel::searchInChannelNames() const {
    return m_searchInChannelNames;
}

void NoDirSortProxyModel::setSearchInChannelNames(bool enabled) {
    if (enabled == m_searchInChannelNames)
        return;

    m_searchInChannelNames = enabled;
    updateSearchTerm();

    emit searchInChannelNamesChanged();
}

bool NoDirSortProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QFileSystemModel *fsm = qobject_cast<QFileSystemModel*>(sourceModel());
    Q_ASSERT(fsm);
    if (!fsm) {
        return false;
    }
    bool asc = sortOrder() == Qt::AscendingOrder ? true : false;

    QFileInfo leftFileInfo  = fsm->fileInfo(left);
    QFileInfo rightFileInfo = fsm->fileInfo(right);

    // If DotAndDot move in the beginning
    if (sourceModel()->data(left).toString() == "..")
        return asc;
    if (sourceModel()->data(right).toString() == "..")
        return !asc;

    // Move dirs up
    if (!leftFileInfo.isDir() && rightFileInfo.isDir()) {
        return !asc;
    }
    if (leftFileInfo.isDir() && !rightFileInfo.isDir()) {
        return asc;
    }

    if (leftFileInfo.isDir() && rightFileInfo.isDir()) {
        // Sort dirs alphabetically
        return leftFileInfo.fileName() < rightFileInfo.fileName();
    }

    // uses file modification date
    return QSortFilterProxyModel::lessThan(left, right);
}

void NoDirSortProxyModel::updateSearchTerm() {
    QString pattern;
    if (!m_searchTerm.isEmpty())
        pattern = ".*" + m_searchTerm + ".*";

    setFilterRegularExpression(QRegularExpression(pattern,
                                                  QRegularExpression::CaseInsensitiveOption));
}

bool NoDirSortProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QRegularExpression re = filterRegularExpression();

    FileSystemModel *fsm = qobject_cast<FileSystemModel *>(sourceModel());
    Q_ASSERT(fsm);
    if (!fsm) {
        return false;
    }

    QModelIndex nameIndex = fsm->index(sourceRow, 0, sourceParent);
    const bool isDir = fsm->hasChildren(nameIndex);

    const QString key = fsm->data(nameIndex, FileSystemModel::FileNameRole).toString();

    if (isDir) {
        return key.contains(allowedDirsPattern);
    }

    QString title = fsm->data(nameIndex, FileSystemModel::TitleRole).toString();

    const bool starred = fsm->isStarred(key);
    if ((starred && !m_searchInStarred) || (!starred && !m_searchInUnstarred))
        return false;

    const bool shortVideo = YaycUtilities::isShortVideo(key);
    if (shortVideo && !m_searchInShorts)
        return false;

    const bool opened = !shortVideo && fsm->duration(key) > 0.;
    if ((opened && !m_searchInOpened) || (!opened && !m_searchInUnopened))
        return false;

    const bool viewed = !shortVideo && fsm->isViewed(key);
    if ((viewed && !m_searchInWatched) || (!viewed && !m_searchInUnwatched))
        return false;

    const bool hasWorkingDir = m_workingDirRoot.isEmpty() || fsm->hasWorkingDir(key, m_workingDirRoot);
    if ((!hasWorkingDir && !m_searchInUnsaved) || (hasWorkingDir && !m_searchInSaved))
        return false;

    if (re.pattern().isEmpty())
        return true;

    const QString channelName =
            fsm->data(nameIndex, FileSystemModel::ChannelNameRole).toString();

    const QString channelId =
            fsm->data(nameIndex, FileSystemModel::ChannelIdRole).toString();

    bool searchInTitles = m_searchInTitles || (!m_searchInTitles && !m_searchInChannelNames);

    bool res = false;
    if (searchInTitles)
        res |= title.contains(re);
    if (m_searchInChannelNames) {
        res |= channelName.contains(re);
        res |= channelId.contains(re);
    }

    return res;
}
