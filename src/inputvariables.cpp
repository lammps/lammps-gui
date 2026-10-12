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

#include "inputvariables.h"

#include <QRegularExpression>
#include <QStringList>

IndexVariableMatch matchIndexVariable(const QString &line)
{
    static const QRegularExpression indexvar(R"(^\s*variable\s+(\w+)\s+index\s+(.*))");

    IndexVariableMatch result;
    QString normalized = line;
    normalized.replace('\t', ' ');

    auto match = indexvar.match(normalized);
    if (match.hasMatch() && (match.lastCapturedIndex() >= 2)) {
        const QString raw = match.captured(2);
        result.valid      = true;
        result.name       = match.captured(1);
        result.value      = raw.trimmed();
        int lead          = 0;
        while ((lead < raw.size()) && raw.at(lead).isSpace())
            ++lead;
        result.valueStart  = match.capturedStart(2) + lead;
        result.valueLength = result.value.size();
    }
    return result;
}

QList<VariableEntry> parseInputVariables(const QString &text)
{
    QString copy   = text;
    const auto doc = copy.replace('\t', ' ').split('\n');
    QStringList known;
    QList<VariableEntry> entries;
    QRegularExpression anyvar(R"(^\s*variable\s+(\w+)\s+(\w+)\s+(.*))");
    QRegularExpression usevar(R"((\$(\w)|\${(\w+)}))");
    QRegularExpression refvar(R"(v_(\w+))");

    for (const auto &line : doc) {

        if (line.isEmpty()) continue;

        // first find variable definitions.
        // index variables are special since they can be overridden from the command line
        const auto index = matchIndexVariable(line);
        auto any         = anyvar.match(line);

        if (index.valid) {
            if (!known.contains(index.name)) {
                entries.append({index.name, index.value, index.value});
                known.append(index.name);
            }
        } else if (any.hasMatch()) {
            if (any.lastCapturedIndex() >= 3) {
                auto name = any.captured(1);
                if (!known.contains(name)) known.append(name);
            }
        }

        // now split line into words and search for use of undefined variables
        auto words = line.split(' ', Qt::SkipEmptyParts);
        for (const auto &word : words) {
            auto use = usevar.match(word);
            auto ref = refvar.match(word);
            if (use.hasMatch()) {
                auto name = use.captured(use.lastCapturedIndex());
                if (!known.contains(name)) {
                    known.append(name);
                    entries.append({name, QString(), QString()});
                }
            }
            if (ref.hasMatch()) {
                auto name = ref.captured(ref.lastCapturedIndex());
                if (!known.contains(name)) known.append(name);
            }
        }
    }
    return entries;
}

QList<VariableEntry> mergeInputVariables(const QList<VariableEntry> &parsed,
                                         const QList<VariableEntry> &previous)
{
    QList<VariableEntry> merged;
    QStringList taken;

    for (auto entry : parsed) {
        const VariableEntry *prev = nullptr;
        for (const auto &p : previous) {
            if (p.name == entry.name) {
                prev = &p;
                break;
            }
        }
        if (prev) {
            // a changed, non-empty script value is the most recent user edit
            // and wins; otherwise keep the current value, which may be an
            // override entered in the Set Variables dialog
            if (entry.scriptValue.isEmpty() || (entry.scriptValue == prev->scriptValue))
                entry.value = prev->value;
        }
        taken.append(entry.name);
        merged.append(entry);
    }

    // keep leftover entries with a value: they are not (or no longer) visible
    // to the parser but may still apply, e.g. to included files
    for (const auto &prev : previous) {
        if (taken.contains(prev.name) || prev.value.isEmpty()) continue;
        taken.append(prev.name);
        merged.append({prev.name, prev.value, QString()});
    }
    return merged;
}

bool isOverridden(const VariableEntry &entry)
{
    if (entry.scriptValue.isEmpty()) return false;
    const QString value = entry.value.trimmed();
    return !value.isEmpty() && (value != entry.scriptValue);
}

// Local Variables:
// c-basic-offset: 4
// End:
