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

#ifndef STDCAPTURE_H
#define STDCAPTURE_H

#include <string>
#include <vector>

/**
 * @brief Capture stdout output to a string buffer
 *
 * This class provides functionality to redirect and capture standard output
 * (stdout) into a string buffer. Used to capture output from LAMMPS library
 * calls for display in the GUI.
 */
class StdCapture {
public:
    /**
     * @brief Constructor - initializes capture buffers
     */
    StdCapture();
    StdCapture(const StdCapture &)            = delete;
    StdCapture(StdCapture &&)                 = delete;
    StdCapture &operator=(const StdCapture &) = delete;
    StdCapture &operator=(StdCapture &&)      = delete;

    /**
     * @brief Destructor - closes the capture pipe descriptors
     */
    ~StdCapture();

    /**
     * @brief Start capturing stdout
     *
     * Redirects stdout to an internal pipe for capture
     */
    void beginCapture();

    /**
     * @brief Stop capturing stdout and restore original stdout
     * @return true if capture was active, false otherwise
     */
    bool endCapture();

    /**
     * @brief Get all captured output and clear the buffer
     * @return String containing all captured output
     */
    std::string getCapture();

    /**
     * @brief Get a chunk of captured output without clearing
     * @return String containing new output since last getChunk call
     */
    std::string getChunk();

    /**
     * @brief Get the buffer usage as a fraction of max buffer size
     * @return Value between 0.0 and 1.0 indicating buffer fullness
     */
    double getBufferUse() const;

    /**
     * @brief Whether stdout could be redirected at all
     * @return true when captured output can reach the caller
     *
     * False means nothing the library writes can ever be shown, and
     * @ref diagnostic() says why.  Worth asking: the failure is otherwise
     * completely silent, because printf() reports success either way and the
     * runtime simply drops the bytes.
     */
    bool isUsable() const { return m_usable; }

    /**
     * @brief Why capture is not available
     * @return One line naming the step that failed, empty while capture works
     */
    const std::string &diagnostic() const { return m_diagnostic; }

    /**
     * @brief Bytes retrieved through getChunk() since the capture began
     * @return Byte count; zero after a whole run means the output was lost
     */
    size_t totalRead() const { return m_totalread; }

    /**
     * @brief Decide which side lost the output of a run that captured nothing
     * @return One line stating whether the redirect still worked at run end,
     *         empty when there is no active, usable capture to probe
     *
     * Performs one more marker round trip while the capture is still active:
     * a marker that comes back proves the redirect held for the whole run
     * (the library's output went elsewhere), one that does not means stdout
     * was re-pointed while the run was underway.  Call before endCapture();
     * any real output drained alongside the marker is preserved and delivered
     * through the following endCapture()/getCapture().
     */
    std::string probeRunEnd();

private:
    /**
     * @brief Pipe file descriptors for capturing output
     */
    enum PIPES { READ, WRITE, PIPE_COUNT };
    // -1, not 0: a process without a console leaves the standard descriptors
    // free, so 0 is one a pipe can legitimately be given.  Initialized here
    // rather than in the constructor body, which has paths that return early.
    int m_pipe[PIPE_COUNT] = {-1, -1}; ///< Pipe file descriptors
    int m_oldStdOut;                   ///< Original stdout file descriptor
    bool m_capturing;                  ///< Flag indicating if capture is active
    std::string m_captured;            ///< Buffer for captured output
    int maxread; ///< High-water mark of bytes read per chunk (for getBufferUse)

    std::vector<char> buf; ///< Internal read buffer

    /// Build a diagnostic carrying the descriptor numbers behind it.
    std::string describe(const char *what) const;

    /// One non-blocking read of the pipe into the buffer.  Returns the number
    /// of bytes read, 0 when there was nothing, or what read() returns on an
    /// error (-1, with errno set).  The one place for the platform difference:
    /// on Windows the pipe cannot be made non-blocking, so it is only read
    /// when it is known to hold data.
    int readPipe();

    /// Prove a freshly set up capture: write a marker through stdout, read it
    /// back out of the pipe, and mark the capture unusable when it does not
    /// return.  Runs from beginCapture(), before anything else writes.
    void verifyCapture();

    bool m_usable = false;       ///< stdout could be redirected into the pipe
    std::string m_diagnostic;    ///< Why it could not, for reporting to the user
    size_t m_totalread = 0;      ///< Bytes handed out via getChunk() this capture
    std::string m_probeleftover; ///< Output drained by probeRunEnd() with its marker
};

#endif
// Local Variables:
// c-basic-offset: 4
// End:
