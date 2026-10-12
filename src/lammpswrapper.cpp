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

#include "lammpswrapper.h"

#include "constants.h"
#include "helpers.h"

#if defined(LAMMPS_GUI_USE_PLUGIN)
#include "liblammpsplugin.h"
#else
#include "library.h"
#endif

#include <cstdio>
#include <cstring>

#include <QFile>

#if defined(LAMMPS_GUI_USE_PLUGIN) && defined(Q_OS_LINUX)
#include <elf.h>
#endif

// Dispatch a LAMMPS C-library function by its base name.  In plugin mode this
// resolves to the dynamically loaded function table; in linked mode to the
// matching lammps_* symbol.  Usage: LMPFN(version)(lammps_handle).
#if defined(LAMMPS_GUI_USE_PLUGIN)
#define LMPFN(fn) (((liblammpsplugin_t *)plugin_handle)->fn)
#else
#define LMPFN(fn) (lammps_##fn)
#endif

LammpsWrapper::LammpsWrapper() : lammps_handle(nullptr)
{
#if defined(LAMMPS_GUI_USE_PLUGIN)
    plugin_handle = nullptr;
#endif
}

LammpsWrapper::~LammpsWrapper()
{
#if defined(LAMMPS_GUI_USE_PLUGIN)
    liblammpsplugin_release(static_cast<liblammpsplugin_t *>(plugin_handle));
    plugin_handle = nullptr;
#endif
}

void LammpsWrapper::open(int narg, char **args)
{
    // since there may only be one LAMMPS instance in LAMMPS-GUI we don't open a second one
    if (lammps_handle) return;
    lammps_handle = LMPFN(open_no_mpi)(narg, args, nullptr);
}

int LammpsWrapper::version()
{
    int val = 0;
    if (lammps_handle) {
        val = LMPFN(version)(lammps_handle);
    }
    return val;
}

int LammpsWrapper::extractSetting(const char *keyword)
{
    int val = 0;
    if (lammps_handle) {
        val = LMPFN(extract_setting)(lammps_handle, keyword);
    }
    return val;
}

void *LammpsWrapper::extractGlobal(const char *keyword)
{
    void *val = nullptr;
    if (lammps_handle) {
        val = LMPFN(extract_global)(lammps_handle, keyword);
    }
    return val;
}

void *LammpsWrapper::extractPair(const char *keyword)
{
    void *val = nullptr;
    if (lammps_handle) {
        val = LMPFN(extract_pair)(lammps_handle, keyword);
    }
    return val;
}

void *LammpsWrapper::extractAtom(const char *keyword)
{
    void *val = nullptr;
    if (lammps_handle) {
        val = LMPFN(extract_atom)(lammps_handle, keyword);
    }
    return val;
}

void *LammpsWrapper::extractCompute(const QString &id, int style, int type)
{
    int mystyle = -1;
    int mytype  = -1;

    switch (style) {
        case GLOBAL_STYLE:
            mystyle = LMP_STYLE_GLOBAL;
            break;
        case ATOM_STYLE:
            mystyle = LMP_STYLE_ATOM;
            break;
        case LOCAL_STYLE:
            mystyle = LMP_STYLE_LOCAL;
            break;
        default:
            mystyle = -1;
            break;
    }
    switch (type) {
        case SCALAR_TYPE:
            mytype = LMP_TYPE_SCALAR;
            break;
        case VECTOR_TYPE:
            mytype = LMP_TYPE_VECTOR;
            break;
        case ARRAY_TYPE:
            mytype = LMP_TYPE_ARRAY;
            break;
        case NUM_ROWS:
            mytype = LMP_SIZE_ROWS;
            break;
        case NUM_COLS:
            mytype = LMP_SIZE_COLS;
            break;
        default:
            mytype = -1;
            break;
    }

    if (lammps_handle) {
        return LMPFN(extract_compute)(lammps_handle, id.toLocal8Bit(), mystyle, mytype);
    }
    return nullptr;
}

void *LammpsWrapper::extractFix(const QString &id, int style, int type, int nrow, int ncol)
{
    int mystyle = -1;
    int mytype  = -1;

    switch (style) {
        case GLOBAL_STYLE:
            mystyle = LMP_STYLE_GLOBAL;
            break;
        case ATOM_STYLE:
            mystyle = LMP_STYLE_ATOM;
            break;
        case LOCAL_STYLE:
            mystyle = LMP_STYLE_LOCAL;
            break;
        default:
            mystyle = -1;
            break;
    }
    switch (type) {
        case SCALAR_TYPE:
            mytype = LMP_TYPE_SCALAR;
            break;
        case VECTOR_TYPE:
            mytype = LMP_TYPE_VECTOR;
            break;
        case ARRAY_TYPE:
            mytype = LMP_TYPE_ARRAY;
            break;
        case NUM_ROWS:
            mytype = LMP_SIZE_ROWS;
            break;
        case NUM_COLS:
            mytype = LMP_SIZE_COLS;
            break;
        default:
            mytype = -1;
            break;
    }

    if (lammps_handle) {
        return LMPFN(extract_fix)(lammps_handle, id.toLocal8Bit(), mystyle, mytype, nrow, ncol);
    }
    return nullptr;
}

int LammpsWrapper::extractVariableDatatype(const QString &keyword)
{
    int type = -1;
    if (lammps_handle) {
        type = LMPFN(extract_variable_datatype)(lammps_handle, keyword.toLocal8Bit());
    }
    switch (type) {
        case LMP_VAR_EQUAL:
            return EQUAL_STYLE;
        case LMP_VAR_ATOM:
            return ATOM_STYLE;
        case LMP_VAR_VECTOR:
            return VECTOR_STYLE;
        case LMP_VAR_STRING:
            return STRING_STYLE;
        default:
            type = -1;
            break;
    }
    return type;
}

// note: equal style and compatible variables only
double LammpsWrapper::extractVariable(const char *keyword)
{
    void *ptr = nullptr;
    if (lammps_handle) {
        ptr = LMPFN(extract_variable)(lammps_handle, keyword, nullptr);
    }
    double val = (ptr) ? *(static_cast<double *>(ptr)) : 0.0;
    LMPFN(free)(ptr);
    return val;
}

int LammpsWrapper::idCount(const char *idtype)
{
    int val = 0;
    if (lammps_handle) {
        val = LMPFN(id_count)(lammps_handle, idtype);
    }
    return val;
}

int LammpsWrapper::hasId(const char *idtype, const char *id)
{
    int val = 0;
    if (lammps_handle) {
        val = LMPFN(has_id)(lammps_handle, idtype, id);
    }
    return val;
}

int LammpsWrapper::idName(const char *keyword, int idx, char *buf, int len)
{
    int val = 0;
    if (lammps_handle) {
        val = LMPFN(id_name)(lammps_handle, keyword, idx, buf, len);
    }
    return val;
}

QString LammpsWrapper::idName(const char *keyword, int idx)
{
    char buf[Cfg::DEFAULT_BUFLEN];
    if (idName(keyword, idx, buf, Cfg::DEFAULT_BUFLEN)) return QString::fromLocal8Bit(buf);
    return {};
}

int LammpsWrapper::styleCount(const char *keyword)
{
    int val = 0;
    if (lammps_handle) {
        val = LMPFN(style_count)(lammps_handle, keyword);
    }
    return val;
}

int LammpsWrapper::styleName(const char *keyword, int idx, char *buf, int len)
{
    int val = 0;
    if (lammps_handle) {
        val = LMPFN(style_name)(lammps_handle, keyword, idx, buf, len);
    }
    return val;
}

QString LammpsWrapper::styleName(const char *keyword, int idx)
{
    char buf[Cfg::DEFAULT_BUFLEN];
    if (styleName(keyword, idx, buf, Cfg::DEFAULT_BUFLEN)) return QString::fromLocal8Bit(buf);
    return {};
}

int LammpsWrapper::variableInfo(int idx, char *buf, int len)
{
    int val = 0;
    if (lammps_handle) {
        val = LMPFN(variable_info)(lammps_handle, idx, buf, len);
    }
    return val;
}

QString LammpsWrapper::variableInfo(int idx)
{
    char buf[Cfg::DEFAULT_BUFLEN];
    if (variableInfo(idx, buf, Cfg::DEFAULT_BUFLEN)) return QString::fromLocal8Bit(buf);
    return {};
}

double LammpsWrapper::getThermo(const char *keyword)
{
    double val = 0.0;
    if (lammps_handle) {
        val = LMPFN(get_thermo)(lammps_handle, keyword);
    }
    return val;
}

void *LammpsWrapper::lastThermo(const char *keyword, int index)
{
    void *ptr = nullptr;
    if (lammps_handle) {
        ptr = LMPFN(last_thermo)(lammps_handle, keyword, index);
    }
    return ptr;
}

bool LammpsWrapper::isRunning()
{
    int val = 0;
    if (lammps_handle) {
        val = LMPFN(is_running)(lammps_handle);
    }
    return val != 0;
}

void LammpsWrapper::command(const QString &input)
{
    if (lammps_handle) {
        LMPFN(command)(lammps_handle, input.toLocal8Bit());
    }
}

void LammpsWrapper::file(const QString &filename)
{
    if (lammps_handle) {
        LMPFN(file)(lammps_handle, filename.toLocal8Bit());
    }
}

void LammpsWrapper::commandsString(const QString &input)
{
    if (lammps_handle) {
        LMPFN(commands_string)(lammps_handle, input.toLocal8Bit());
    }
}

// may be called with null handle. returns global error then.
bool LammpsWrapper::hasError() const
{
    return LMPFN(has_error)(lammps_handle) != 0;
}

// may be called with null handle. returns global error then.
int LammpsWrapper::getLastErrorMessage(char *buf, int buflen)
{
    return LMPFN(get_last_error_message)(lammps_handle, buf, buflen);
}

QString LammpsWrapper::lastErrorMessage()
{
    if (!hasError()) return {};
    char buf[Cfg::DEFAULT_BUFLEN];
    getLastErrorMessage(buf, Cfg::DEFAULT_BUFLEN);
    return QString::fromLocal8Bit(buf);
}

void LammpsWrapper::forceTimeout()
{
    if (lammps_handle) LMPFN(force_timeout)(lammps_handle);
}

void LammpsWrapper::close()
{
#if defined(LAMMPS_GUI_USE_PLUGIN)
    if (lammps_handle && plugin_handle) ((liblammpsplugin_t *)plugin_handle)->close(lammps_handle);
#else
    if (lammps_handle) lammps_close(lammps_handle);
#endif
    lammps_handle = nullptr;
}

void LammpsWrapper::finalize()
{
    if (lammps_handle) {
        LMPFN(close)(lammps_handle);
        LMPFN(mpi_finalize)();
        LMPFN(kokkos_finalize)();
        LMPFN(python_finalize)();
        // otherwise isOpen() reports an instance that no longer exists and a
        // later close() would close the stale handle a second time
        lammps_handle = nullptr;
    }
}

bool LammpsWrapper::configHasPackage(const char *package) const
{
    return LMPFN(config_has_package)(package) != 0;
}

bool LammpsWrapper::configAccelerator(const char *package, const char *category,
                                      const char *setting) const
{
    return LMPFN(config_accelerator)(package, category, setting) != 0;
}

bool LammpsWrapper::configHasCurlSupport() const
{
    return LMPFN(config_has_curl_support)() != 0;
}

bool LammpsWrapper::configHasOmpSupport() const
{
    return LMPFN(config_has_omp_support)() != 0;
}

bool LammpsWrapper::configHasPngSupport() const
{
    return LMPFN(config_has_png_support)() != 0;
}

bool LammpsWrapper::configHasJpegSupport() const
{
    return LMPFN(config_has_jpeg_support)() != 0;
}

bool LammpsWrapper::hasGpuDevice() const
{
    return LMPFN(has_gpu_device)() != 0;
}

#undef LMPFN

#if defined(LAMMPS_GUI_USE_PLUGIN)
bool LammpsWrapper::hasPlugin() const
{
    return true;
}

// Detect an obviously truncated or corrupt ELF shared object before it is
// handed to dlopen().  A partial file -- for example from an interrupted
// download -- makes the dynamic linker dereference relocation data past the end
// of the file and crash the entire process instead of failing cleanly.  The
// check is deliberately fail-safe: it returns true only on positive evidence of
// truncation and otherwise lets the loader proceed.
static bool pluginFileLooksTruncated(const QString &libfile)
{
#if defined(Q_OS_LINUX)
    QFile f(libfile);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const qint64 filesize = f.size();

    Elf64_Ehdr ehdr;
    if (f.read(reinterpret_cast<char *>(&ehdr), sizeof(ehdr)) != sizeof(ehdr)) return false;

    // only validate native 64-bit little-endian ELF objects; anything else the
    // dynamic loader rejects on its own without crashing
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) return false;
    if ((ehdr.e_ident[EI_CLASS] != ELFCLASS64) || (ehdr.e_ident[EI_DATA] != ELFDATA2LSB))
        return false;

    // the section header table is conventionally at the very end of the file, so
    // a table that runs past EOF is a reliable indication of truncation
    if (ehdr.e_shoff != 0) {
        const qint64 shend = static_cast<qint64>(ehdr.e_shoff) +
                             static_cast<qint64>(ehdr.e_shnum) * ehdr.e_shentsize;
        if (shend > filesize) return true;
    }

    // every loadable segment must lie entirely within the file
    if ((ehdr.e_phoff != 0) && (ehdr.e_phnum > 0)) {
        const qint64 phend = static_cast<qint64>(ehdr.e_phoff) +
                             static_cast<qint64>(ehdr.e_phnum) * ehdr.e_phentsize;
        if (phend > filesize) return true;
        if (!f.seek(ehdr.e_phoff)) return false;
        for (unsigned i = 0; i < ehdr.e_phnum; ++i) {
            Elf64_Phdr phdr;
            if (f.read(reinterpret_cast<char *>(&phdr), sizeof(phdr)) != sizeof(phdr)) return true;
            if (phdr.p_type != PT_LOAD) continue;
            const qint64 segend =
                static_cast<qint64>(phdr.p_offset) + static_cast<qint64>(phdr.p_filesz);
            if (segend > filesize) return true;
        }
    }
    return false;
#else
    Q_UNUSED(libfile);
    return false;
#endif
}

bool LammpsWrapper::loadLib(const QString &libfile)
{
    // reject an obviously truncated or corrupt library up front; handing such a
    // file to dlopen() would crash the process inside the dynamic linker
    if (pluginFileLooksTruncated(libfile)) {
        fprintf(stderr,
                "LAMMPS library file %s rejected.\n"
                "The file appears truncated or corrupted (e.g. from an incomplete "
                "download). Please remove it and install the library again.\n",
                libfile.toLocal8Bit().constData());
        return false;
    }

    if (plugin_handle) {
        close();
        liblammpsplugin_release(static_cast<liblammpsplugin_t *>(plugin_handle));
    }
    plugin_handle = liblammpsplugin_load(libfile.toLocal8Bit());
    if (!plugin_handle) return false;
    auto *lmp = static_cast<liblammpsplugin_t *>(plugin_handle);

    // check if ABI matches
    if (lmp->abiversion != LAMMPSPLUGIN_ABI_VERSION) {
        // cache the ABI version before releasing lmp; the release frees it
        const int abiversion = lmp->abiversion;
        liblammpsplugin_release(lmp);
        plugin_handle = nullptr;
        fprintf(stderr, "LAMMPS library file %s rejected.\nIncompatible ABI: %d vs %d\n",
                libfile.toLocal8Bit().constData(), abiversion, LAMMPSPLUGIN_ABI_VERSION);
        return false;
    }

    // check if all required recently added library functions are present; on
    // failure unload the library again so no unusable handle stays behind
#define CHECKSYM(symbol)                                                          \
    if (lmp->symbol == NULL) {                                                    \
        fprintf(stderr, "LAMMPS library file %s is missing lammps_%s function\n", \
                libfile.toLocal8Bit().constData(), #symbol);                      \
        liblammpsplugin_release(lmp);                                             \
        plugin_handle = nullptr;                                                  \
        return false;                                                             \
    }

    CHECKSYM(get_thermo);
    CHECKSYM(last_thermo);
    CHECKSYM(config_has_curl_support);
    CHECKSYM(config_has_omp_support);
    CHECKSYM(extract_pair);

    // check minimum required version
    QString lmpversion;
    auto *ptr = static_cast<const char *>(lmp->extract_global(nullptr, "lammps_version"));
    if (ptr) lmpversion = ptr;

    // found a suitable version
    if (!lmpversion.isEmpty() && (dateCompare(lmpversion, Cfg::MIN_LAMMPS_VERSION_STR) >= 0))
        return true;

    // the library loads but is too old: unload it again for consistency with
    // the other failure paths
    liblammpsplugin_release(lmp);
    plugin_handle = nullptr;
    return false;
}
#else
bool LammpsWrapper::hasPlugin() const
{
    return false;
}

bool LammpsWrapper::loadLib(const QString &)
{
    return true;
}
#endif

// Local Variables:
// c-basic-offset: 4
// End:
