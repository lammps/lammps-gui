// -*- c++ -*- /////////////////////////////////////////////////////////////////////////
// LAMMPS-GUI - A Graphical Tool to Learn and Explore the LAMMPS MD Simulation Software
//
// Copyright (c) 2023, 2024, 2025, 2026  Axel Kohlmeyer
//
// Documentation: https://lammps-gui.lammps.org/
// Contact: akohlmey@gmail.com
//
// This software is distributed under the GNU General Public License version 2 or later.
////////////////////////////////////////////////////////////////////////////////////////

#ifndef URLDOWNLOADER_H
#define URLDOWNLOADER_H

#include <QByteArray>
#include <QPointer>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QWidget;

/**
 * @brief Utility class for downloading files over HTTPS
 *
 * URLDownloader provides a synchronous interface for downloading files from
 * HTTPS URLs.  It respects the "https_proxy" preference setting (or the
 * https_proxy environment variable) when configured.
 *
 * @see Preferences for the proxy setting
 */
class URLDownloader {
public:
    /**
     * @brief Construct a new URLDownloader
     * @param parent Optional parent widget for progress dialogs
     */
    explicit URLDownloader(QWidget *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~URLDownloader();

    URLDownloader(const URLDownloader &)            = delete;
    URLDownloader(URLDownloader &&)                 = delete;
    URLDownloader &operator=(const URLDownloader &) = delete;
    URLDownloader &operator=(URLDownloader &&)      = delete;

    /**
     * @brief Download a file from the given HTTPS URL to a local file
     *
     * Performs a synchronous (blocking) download of the resource at \p url,
     * writing the result to the local file \p file.  Respects the configured
     * HTTPS proxy setting.  Optionally display a dialog with the downloaded
     * URL and the location of the downloaded file.
     *
     * When a SHA256SUMS file is available in the same remote directory, the
     * checksum of the downloaded data is verified *before* it replaces an
     * existing file, so a corrupted download never clobbers a working file.
     *
     * @param url   The HTTPS URL to download from
     * @param file  The local file path to write to
     * @param showDialog  Display a dialog with the downloaded URL and target file location while
     * downloading
     * @param keepBackup  Rename an existing target file to a backup name instead of replacing
     * it in place; required to update a shared library that is currently loaded on Windows,
     * where a loaded library can be renamed but not deleted or overwritten.  The caller is
     * responsible for removing the backup file eventually (LAMMPS-GUI does this at launch).
     * @return true if the download completed successfully, false otherwise
     */
    bool download(const QString &url, const QString &file, bool showDialog = false,
                  bool keepBackup = false);

    /**
     * @brief Return the last error message
     * @return Human-readable error description or empty string
     */
    QString errorString() const { return lastError; }

    /**
     * @brief Abort the current download and any further ones on this instance
     *
     * Safe to call from a slot triggered while download() blocks in its event
     * loop (e.g. the Cancel button of a progress dialog).  The in-flight
     * request is aborted and all subsequent download() calls on this instance
     * fail immediately, so one cancellation stops a whole batch of downloads.
     */
    void abort();

    /** @brief Return whether the download was canceled via abort() */
    bool wasAborted() const { return aborted; }

    /**
     * @brief Return the remote SHA-256 checksum for a given URL
     *
     * Fetches the SHA256SUMS file from the same remote directory as the resource at \p url,
     * and returns the expected hex-hash for the resource's filename.
     *
     * @param url   The HTTPS URL of the resource
     * @return      Hex-hash string or empty string if not found or on error
     */
    QString getRemoteChecksum(const QString &url);

    /**
     * @brief Compute the local SHA-256 checksum for a given file
     *
     * @param file  The local file path
     * @return      Hex-hash string or empty string on error
     */
    static QString getLocalChecksum(const QString &file);

private:
    void configureProxy();
    bool verifyChecksum(const QString &url, const QByteArray &data);

    /**
     * @brief Fetch raw content from a given HTTPS URL
     *
     * Performs a synchronous (blocking) GET request to the given URL and
     * returns the response body as a QByteArray. Used by getRemoteChecksum().
     *
     * @param url  The HTTPS URL to fetch from
     * @return     Response body or empty QByteArray on error
     */
    QByteArray fetchRawContent(const QString &url);

    QNetworkAccessManager *manager;       ///< Qt network access manager
    QWidget *parentWidget;                ///< Parent widget for dialogs
    QString lastError;                    ///< Last error message
    QPointer<QNetworkReply> currentReply; ///< In-flight request, for abort()
    bool aborted     = false;             ///< Set by abort(); never reset
    int stallTimeout = 0;                 ///< Stall timeout in seconds, from the preferences
};

#endif // URLDOWNLOADER_H

// Local Variables:
// c-basic-offset: 4
// End:
