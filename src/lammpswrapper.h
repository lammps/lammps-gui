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

#ifndef LAMMPSWRAPPER_H
#define LAMMPSWRAPPER_H

#include <QString>

/**
 * @brief C++ wrapper for the LAMMPS C library interface
 *
 * This class provides a C++-oriented interface to the LAMMPS library,
 * routing all library calls through a unified API. It manages
 * the LAMMPS instance and handles dynamic loading of the library
 * when the application is built in plugin mode.
 */
class LammpsWrapper {
public:
    /// Constants for variable styles
    enum StyleConst { EQUAL_STYLE = 0, ATOM_STYLE, VECTOR_STYLE, STRING_STYLE };
    /// Constants for data scopes
    enum ScopeConst { GLOBAL_STYLE = 0, DUMMY /* = ATOM_STYLE */, LOCAL_STYLE };
    /// Constants for data types
    enum TypeConst { SCALAR_TYPE = 0, VECTOR_TYPE, ARRAY_TYPE, NUM_ROWS, NUM_COLS };

    /**
     * @brief Constructor - initializes wrapper
     */
    LammpsWrapper();

    /**
     * @brief Destructor
     *
     * Does not close an open LAMMPS instance. Callers must invoke close()
     * explicitly before destroying the wrapper. In plugin mode the handle
     * to the dynamically loaded LAMMPS library is released.
     */
    ~LammpsWrapper();

    LammpsWrapper(const LammpsWrapper &)            = delete;
    LammpsWrapper(LammpsWrapper &&)                 = delete;
    LammpsWrapper &operator=(const LammpsWrapper &) = delete;
    LammpsWrapper &operator=(LammpsWrapper &&)      = delete;

public:
    /**
     * @brief Create a new LAMMPS instance
     * @param nargs Number of command-line arguments
     * @param args Command-line arguments array
     */
    void open(int nargs, char **args);

    /**
     * @brief Close the LAMMPS instance
     */
    void close();

    /**
     * @brief Finalize MPI (if used) and close LAMMPS
     */
    void finalize();

    /**
     * @brief Process commands from a LAMMPS input file
     * @param fname Filename as Qt-style QString
     */
    void file(const QString &fname);

    /**
     * @brief Execute a single LAMMPS command
     * @param cmd Command string as Qt-style QString
     */
    void command(const QString &cmd);

    /**
     * @brief Execute multiple LAMMPS commands from a string
     * @param cmd Commands string with newlines as Qt-style QString
     */
    void commandsString(const QString &cmd);

    /**
     * @brief Force a timeout condition in LAMMPS
     */
    void forceTimeout();

    /**
     * @brief Get LAMMPS version number
     * @return Version number as integer (YYYYMMDD format)
     */
    [[nodiscard]] int version();

    /**
     * @brief Extract a global setting from LAMMPS
     * @param keyword Setting name to extract
     * @return Integer value of the setting
     */
    int extractSetting(const char *keyword);

    /**
     * @brief Extract a pointer to global data from LAMMPS
     * @param keyword Name of global data to extract
     * @return Pointer to the data
     */
    void *extractGlobal(const char *keyword);

    /**
     * @brief Extract pair style data from LAMMPS
     * @param keyword Name of pair data to extract
     * @return Pointer to the pair data cast to a void pointer
     */
    void *extractPair(const char *keyword);

    /**
     * @brief Extract atom data from LAMMPS
     * @param keyword Name of atom data to extract
     * @return Pointer to the atom data cast to void pointer
     */
    void *extractAtom(const char *keyword);

    /**
     * @brief Extract data from a compute from LAMMPS
     * @param id compute id as a QString
     * @param style style of data to extract
     * @param type type of data to extract
     * @return data cast to a void pointer
     */
    void *extractCompute(const QString &id, int style, int type);

    /**
     * @brief Extract data from a fix from LAMMPS
     * @param id fix id as a QString
     * @param style style of data to extract
     * @param type type of data to extract
     * @param nrow row index (only for global)
     * @param ncol column index (only for global)
     * @return data cast to a void pointer. Must be freed for global elements
     */
    void *extractFix(const QString &id, int style, int type, int nrow, int ncol);

    /**
     * @brief Extract a variable value from LAMMPS
     * @param keyword Variable name to extract
     * @return Value of the variable as double
     */
    double extractVariable(const char *keyword);

    /**
     * @brief Extract style of a variable from LAMMPS
     * @param keyword variable name as a QString
     * @return Value type of variable as integer
     */
    int extractVariableDatatype(const QString &keyword);

    /**
     * @brief Check if a compute/fix/variable ID exists
     * @param idtype Type of ID ("compute", "fix", "variable")
     * @param id The ID to check
     * @return 1 if exists, 0 otherwise
     */
    int hasId(const char *idtype, const char *id);

    /**
     * @brief Get count of IDs of a specific type
     * @param idtype Type of ID ("compute", "fix", "variable", "group")
     * @return Number of IDs of that type
     */
    int idCount(const char *idtype);

    /**
     * @brief Get the name of an ID by index
     * @param idtype Type of ID ("compute", "fix", "variable", "group", ...)
     * @param idx Index of the ID
     * @return The ID name, or an empty string on error
     */
    QString idName(const char *idtype, int idx);

    /**
     * @brief Get count of styles of a specific type
     * @param keyword Type of style ("compute", "fix", "pair", etc.)
     * @return Number of available styles
     */
    int styleCount(const char *keyword);

    /**
     * @brief Get the name of a style by index
     * @param keyword Type of style ("command", "pair", "fix", ...)
     * @param idx Index of the style
     * @return The style name, or an empty string on error
     */
    QString styleName(const char *keyword, int idx);

    /**
     * @brief Get the name of a variable by index
     * @param idx Variable index
     * @return The variable name, or an empty string on error
     */
    QString variableInfo(int idx);

    /**
     * @brief Get current value of a thermodynamic quantity
     * @param keyword Thermo keyword
     * @return Value of the thermo quantity
     */
    double getThermo(const char *keyword);

    /**
     * @brief Get a specific value from last thermo output
     * @param keyword Thermo keyword
     * @param idx Index for vector quantities
     * @return Pointer to the value
     */
    void *lastThermo(const char *keyword, int idx);

    /**
     * @brief Typed read of a cached last-thermo value
     * @tparam T scalar type the thermo value points to (int, int64_t, double)
     * @param keyword Thermo keyword ("step", "num", "type", "data", ...)
     * @param idx Index for vector quantities
     * @return The dereferenced value, or a value-initialized T if the query returns null
     */
    template <typename T> T lastThermoAs(const char *keyword, int idx)
    {
        void *ptr = lastThermo(keyword, idx);
        return ptr ? *static_cast<T *>(ptr) : T{};
    }

    /**
     * @brief Read a string-valued cached last-thermo entry
     * @param keyword Thermo keyword that returns text (e.g. "keyword", "imagename")
     * @param idx Index for vector quantities
     * @return The value as a QString (empty if unavailable)
     */
    QString lastThermoString(const char *keyword, int idx)
    {
        return QString::fromLocal8Bit(static_cast<const char *>(lastThermo(keyword, idx)));
    }

    /**
     * @brief Check if LAMMPS instance is open
     * @return true if LAMMPS is initialized, false otherwise
     */
    [[nodiscard]] bool isOpen() const { return lammps_handle != nullptr; }

    /**
     * @brief Check if LAMMPS is currently executing a run
     * @return true if running, false otherwise
     */
    [[nodiscard]] bool isRunning();

    /**
     * @brief Check if LAMMPS has encountered an error
     * @return true if error occurred, false otherwise
     */
    [[nodiscard]] bool hasError() const;

    /**
     * @brief Get the last error message from LAMMPS
     * @param errorbuf Buffer to store error message
     * @param buflen Length of buffer
     * @return Error type code
     */
    int getLastErrorMessage(char *errorbuf, int buflen);

    /**
     * @brief Get the last error message from LAMMPS as a string
     * @return The error text, or an empty string if no error is pending
     *
     * Convenience wrapper around getLastErrorMessage() that manages the
     * character buffer internally. Retrieving the message also clears the
     * pending error state in LAMMPS.
     */
    QString lastErrorMessage();

    /**
     * @brief Check if an accelerator package is available
     * @param package Package name
     * @param category Category name
     * @param setting Setting name
     * @return true if available, false otherwise
     */
    [[nodiscard]] bool configAccelerator(const char *package, const char *category,
                                         const char *setting) const;

    /**
     * @brief Check if a package is included in LAMMPS build
     * @param pkg Package name
     * @return true if included, false otherwise
     */
    [[nodiscard]] bool configHasPackage(const char *pkg) const;

    /**
     * @brief Check if LAMMPS was built with CURL support
     * @return true if CURL is available, false otherwise
     */
    [[nodiscard]] bool configHasCurlSupport() const;

    /**
     * @brief Check if LAMMPS was built with OpenMP support
     * @return true if OpenMP is available, false otherwise
     */
    [[nodiscard]] bool configHasOmpSupport() const;

    /**
     * @brief Check if LAMMPS was compiled with PNG format image support
     * @return true if PNG image format support is available, false if not
     */
    [[nodiscard]] bool configHasPngSupport() const;

    /**
     * @brief Check if LAMMPS was compiled with JPEG format image support
     * @return true if JPEG image format support is available, false if not
     */
    [[nodiscard]] bool configHasJpegSupport() const;

    /**
     * @brief Check if GPU device is available for GPU package
     * @return true if GPU device found, false otherwise
     */
    [[nodiscard]] bool hasGpuDevice() const;

    /**
     * @brief Load LAMMPS shared library (plugin mode)
     * @param fname Library filename (QString version)
     * @return true on success, false on failure
     */
    bool loadLib(const QString &fname);

    /**
     * @brief Check if running in plugin mode
     * @return true if plugin mode enabled, false if linked mode
     */
    [[nodiscard]] bool hasPlugin() const;

private:
    /// @name Low-level char-buffer variants behind the QString-returning overloads
    /// @{
    int idName(const char *idtype, int idx, char *buf, int buflen);
    int styleName(const char *keyword, int idx, char *buf, int buflen);
    int variableInfo(int idx, char *buf, int buflen);
    /// @}

    void *lammps_handle; ///< Handle to LAMMPS instance
#if defined(LAMMPS_GUI_USE_PLUGIN)
    void *plugin_handle; ///< Handle to dynamically loaded LAMMPS library
#endif
};
#endif

// Local Variables:
// c-basic-offset: 4
// End:
