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

// adapted from: https://stackoverflow.com/questions/5419356/redirect-stdout-stderr-to-a-string

#include "stdcapture.h"
#include "helpers.h"

#if defined(Q_OS_WIN32)
#include <io.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define dup _dup
#define dup2 _dup2
#define fileno _fileno
#define close _close
#define read _read
#else
#include <unistd.h>
#endif

#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <thread>

#if defined(Q_OS_WIN32)
// Check if a pipe has data available for reading without blocking.
// Uses PeekNamedPipe() instead of _eof() because _eof() relies on _lseek()
// internally to check the file position.  Since pipes are not seekable,
// _eof() returns -1 (error) on pipe file descriptors under the MSVC C runtime,
// which causes the caller to skip reading entirely and capture nothing.
// PeekNamedPipe() is the proper Win32 API for non-blocking pipe inspection.
static bool pipe_has_data(int fd)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD available = 0;
    if (!PeekNamedPipe(h, nullptr, 0, nullptr, &available, nullptr)) return false;
    return available > 0;
}
#endif

namespace {
constexpr int bufSize = (1 << 16) + 1;
} // namespace

// The numbers matter more than the words when this has to be diagnosed from a
// user's screenshot, so every message carries them.
std::string StdCapture::describe(const char *what) const
{
    return std::string(what) + " (stdout fd " + std::to_string(fileno(stdout)) + ", pipe " +
           std::to_string(m_pipe[READ]) + "/" + std::to_string(m_pipe[WRITE]) + ", saved " +
           std::to_string(m_oldStdOut) + ")";
}

StdCapture::StdCapture() : m_oldStdOut(-1), m_capturing(false), maxread(0), buf(bufSize)
{
#if defined(Q_OS_WIN32)
    // A Windows GUI-subsystem process is started without a console, and the C
    // runtime then leaves stdout with no file descriptor at all: fileno(stdout)
    // is -2 and every attempt to redirect it fails.  Nothing reports this --
    // printf() still claims to have written the bytes, the runtime simply drops
    // them -- so the whole capture silently yields an empty Output window.  It
    // has therefore only ever worked when the program was started from a
    // console.  Attaching stdout to the null device gives it a real descriptor,
    // which is all that redirecting it needs; what is written there is replaced
    // by the pipe in beginCapture() and never reaches the device.
    if (fileno(stdout) < 0) {
        if (!freopen("NUL", "w", stdout)) {
            m_diagnostic = "stdout has no file descriptor and could not be attached to NUL";
            return;
        }
    }
#endif
    // make stdout unbuffered so that we don't need to flush the stream
    setvbuf(stdout, nullptr, _IONBF, 0);

#if defined(Q_OS_WIN32)
    if (_pipe(m_pipe, 65536, O_BINARY) == -1) {
        m_diagnostic = "the capture pipe could not be created";
        return;
    }
#else
    if (pipe(m_pipe) == -1) {
        m_diagnostic = "the capture pipe could not be created";
        return;
    }
    if (fcntl(m_pipe[READ], F_SETFL, fcntl(m_pipe[READ], F_GETFL) | O_NONBLOCK) == -1) {
        m_diagnostic = "the capture pipe could not be made non-blocking";
        return;
    }
#endif
    m_oldStdOut = dup(fileno(stdout));
    if (m_oldStdOut == -1) {
        m_diagnostic = describe("stdout could not be duplicated");
        return;
    }
    m_usable = true;
}

StdCapture::~StdCapture()
{
    notifyCaptureState(false);
    if (m_oldStdOut >= 0) close(m_oldStdOut);
    if (m_pipe[READ] >= 0) close(m_pipe[READ]);
    if (m_pipe[WRITE] >= 0) close(m_pipe[WRITE]);
}

void StdCapture::beginCapture()
{
    if (m_capturing) endCapture();
    if (isStdoutSilenced()) restoreStdout();
    if (m_pipe[WRITE] < 0) return; // no pipe to capture into
    if (dup2(m_pipe[WRITE], fileno(stdout)) == -1) {
        // the library's output now goes wherever stdout pointed, which in a
        // process without a console is nowhere at all
        m_usable     = false;
        m_diagnostic = describe("stdout could not be redirected into the capture pipe");
        return;
    }
    m_capturing = true;
    maxread     = 0;
    m_totalread = 0;
    notifyCaptureState(true);
    verifyCapture();
}

// A capture that does not work leaves no trace of its own: printf() reports
// success and the runtime drops the bytes.  Every probe of the individual steps
// can pass and the assembled whole still fail, so prove the plumbing whenever
// it is set up: write a marker through the very stream the library will use and
// read it back out of the pipe.  This runs in the gap between the redirect and
// the start of the run, when nothing else writes, so the pipe carries exactly
// the marker and is drained again before real output arrives.
void StdCapture::verifyCapture()
{
    if (!m_usable || !m_capturing) return;

    static constexpr char marker[] = "__LGUI_CAPTURE_TEST__\n";
    constexpr int len              = static_cast<int>(sizeof(marker)) - 1;
    printf("%s", marker); // its return value is the known lie; the read-back is the test

    // the write is a same-process pipe write of a few bytes, so it is visible
    // at once; the loop only covers scheduling noise
    std::string got;
    for (int wait = 0; wait < 100; ++wait) {
        int bytesRead = 0;
#if defined(Q_OS_WIN32)
        if (pipe_has_data(m_pipe[READ])) bytesRead = read(m_pipe[READ], buf.data(), bufSize - 1);
#else
        bytesRead = read(m_pipe[READ], buf.data(), bufSize - 1);
#endif
        if (bytesRead > 0) {
            buf[bytesRead] = 0;
            got += buf.data();
        }
        if (static_cast<int>(got.size()) >= len) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (got != marker) {
        m_usable = false;
        m_diagnostic =
            describe(got.empty() ? "the test marker did not come back out of the capture pipe"
                                 : "the capture pipe returned something other than the test "
                                   "marker");
    }
}

// The counterpart at the other end: when a whole run yielded nothing, decide
// which side lost it.  One more marker round trip through the still-active
// capture tells the two failure modes apart: a marker that comes back proves
// the redirect held for the entire run -- so the library's output went
// somewhere else -- and one that does not means stdout was re-pointed while
// the run was underway.  Any real output drained along with the marker is
// kept and handed back through the next endCapture()/getCapture().
std::string StdCapture::probeRunEnd()
{
    if (!m_capturing || !m_usable) return {};

    static constexpr char marker[] = "__LGUI_CAPTURE_TEST__\n";
    printf("%s", marker);

    std::string got;
    for (int wait = 0; wait < 100; ++wait) {
        int bytesRead = 0;
#if defined(Q_OS_WIN32)
        if (pipe_has_data(m_pipe[READ])) bytesRead = read(m_pipe[READ], buf.data(), bufSize - 1);
#else
        bytesRead = read(m_pipe[READ], buf.data(), bufSize - 1);
#endif
        if (bytesRead > 0) {
            buf[bytesRead] = 0;
            got += buf.data();
        }
        if (got.find(marker) != std::string::npos) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const auto pos = got.find(marker);
    if (pos == std::string::npos) {
        m_probeleftover = std::move(got);
        return describe("stdout no longer feeds the capture pipe: a marker written at the"
                        " end of the run did not come back");
    }
    m_probeleftover = got.substr(0, pos) + got.substr(pos + sizeof(marker) - 1);
    return describe("the capture stayed intact for the whole run -- a marker written at"
                    " its end came back -- so the library's output went elsewhere");
}

bool StdCapture::endCapture()
{
    if (!m_capturing) return false;
    notifyCaptureState(false);
    if (m_oldStdOut >= 0) dup2(m_oldStdOut, fileno(stdout));
    // not clear(): keep what an end-of-run probe drained alongside its marker
    m_captured = m_probeleftover;
    m_probeleftover.clear();

    // Whatever wrote into the pipe has returned by the time this is called, so
    // what is in it is all there will be: the first read that would block means
    // the pipe is drained.  Waiting for more instead cost a full second on
    // every empty pipe.  Only a read cut short by a signal is tried again.
    int bytesRead;
    bool interrupted;
    int retries = 100;

    do {
        bytesRead   = 0;
        interrupted = false;

#if defined(Q_OS_WIN32)
        if (pipe_has_data(m_pipe[READ])) {
            bytesRead = read(m_pipe[READ], buf.data(), bufSize - 1);
        }
#else
        bytesRead = read(m_pipe[READ], buf.data(), bufSize - 1);
#endif
        if (bytesRead > 0) {
            buf[bytesRead] = 0;
            m_captured += buf.data();
        } else if (bytesRead < 0) {
            interrupted = (errno == EINTR) && (--retries > 0);
        }
    } while (interrupted || (bytesRead == (bufSize - 1)));
    m_capturing = false;
    return true;
}

std::string StdCapture::getChunk()
{
    if (!m_capturing) return {};
    int bytesRead = 0;
    buf[0]        = '\0';

#if defined(Q_OS_WIN32)
    if (pipe_has_data(m_pipe[READ])) {
        bytesRead = read(m_pipe[READ], buf.data(), bufSize - 1);
    }
#else
    bytesRead = read(m_pipe[READ], buf.data(), bufSize - 1);
#endif
    if (bytesRead > 0) {
        buf[bytesRead] = '\0';
        m_totalread += bytesRead;
    }
    maxread = (maxread > bytesRead) ? maxread : bytesRead;
    // by length, not up to a NUL: no scan of the buffer, and a stray NUL byte
    // in the output cannot swallow what follows it
    return {buf.data(), static_cast<std::size_t>(bytesRead > 0 ? bytesRead : 0)};
}

double StdCapture::getBufferUse() const
{
    return static_cast<double>(maxread) / static_cast<double>(bufSize - 1);
}

std::string StdCapture::getCapture()
{
    std::string::size_type idx = m_captured.find_last_not_of("\r\n");
    if (idx == std::string::npos) {
        return m_captured;
    }
    return m_captured.substr(0, idx + 1);
}

// Local Variables:
// c-basic-offset: 4
// End:
