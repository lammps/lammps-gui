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

#include "shellaliases.h"

#include "constants.h"
#include "helpers.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

// QSettings arrays leave a "size" entry behind, which is the only way to tell a
// table the user emptied on purpose from one that was never written at all.
QString sizeKey()
{
    return Keys::ALIASES + QStringLiteral("/size");
}

} // namespace

QList<ShellAlias> ShellAliases::defaults()
{
    // -C is the multi-column listing that ls produces by itself only when its
    // output is a terminal, -F marks directories, and -a is what makes the
    // listing complete; -l for the long form of the same
    return {{QStringLiteral("ls"), QStringLiteral("ls -aCF")},
            {QStringLiteral("ll"), QStringLiteral("ls -laCF")}};
}

QList<ShellAlias> ShellAliases::aliases()
{
    QSettings settings;
    if (!settings.contains(sizeKey())) return defaults();

    QList<ShellAlias> list;
    const int count = settings.beginReadArray(Keys::ALIASES);
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        const QString name = settings.value(Keys::NAME).toString().trimmed();
        if (!name.isEmpty()) list.append({name, settings.value(Keys::VALUE).toString()});
    }
    settings.endArray();
    return list;
}

ShellAliases::ShellAliases(QWidget *parent) : QDialog(parent), table(new QTableWidget(0, 2, this))
{
    setWindowTitle("LAMMPS-GUI: Command Window Aliases");

    auto *explain =
        new QLabel("These aliases are defined in every shell the command window starts.\n"
                   "They exist because the shell reads a pipe and not a terminal, so any\n"
                   "part of your start-up file guarded by a test for one is skipped, and\n"
                   "programs like \"ls\" drop the formatting they keep for a terminal.");
    explain->setWordWrap(true);

    table->setHorizontalHeaderLabels({"Alias", "Expands to"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    setRows(aliases());

    auto *add    = new QPushButton("&Add");
    auto *remove = new QPushButton("&Remove");
    auto *reset  = new QPushButton("Restore &Defaults");
    connect(add, &QPushButton::clicked, this, &ShellAliases::addRow);
    connect(remove, &QPushButton::clicked, this, &ShellAliases::removeRow);
    connect(reset, &QPushButton::clicked, this, &ShellAliases::resetDefaults);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addWidget(reset);
    buttons->addStretch(1);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    styleDialogButtons(box);
    connect(box, &QDialogButtonBox::accepted, this, &ShellAliases::accept);
    connect(box, &QDialogButtonBox::rejected, this, &ShellAliases::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(explain);
    layout->addWidget(table);
    layout->addLayout(buttons);
    layout->addWidget(box);

    resize(Cfg::ALIASES_DEFAULT_WIDTH, Cfg::ALIASES_DEFAULT_HEIGHT);
}

void ShellAliases::setRows(const QList<ShellAlias> &list)
{
    table->setRowCount(0);
    for (const auto &alias : list) {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(alias.first));
        table->setItem(row, 1, new QTableWidgetItem(alias.second));
    }
}

QList<ShellAlias> ShellAliases::rows() const
{
    QList<ShellAlias> list;
    for (int row = 0; row < table->rowCount(); ++row) {
        const auto *name = table->item(row, 0);
        const auto *body = table->item(row, 1);
        if (!name || name->text().trimmed().isEmpty()) continue;
        list.append({name->text().trimmed(), body ? body->text().trimmed() : QString()});
    }
    return list;
}

void ShellAliases::addRow()
{
    const int row = table->rowCount();
    table->insertRow(row);
    table->setItem(row, 0, new QTableWidgetItem);
    table->setItem(row, 1, new QTableWidgetItem);
    table->setCurrentCell(row, 0);
    table->editItem(table->item(row, 0));
}

void ShellAliases::removeRow()
{
    // back to front, so the rows still to be removed keep their numbers
    const auto selected = table->selectionModel()->selectedRows();
    QList<int> victims;
    for (const auto &index : selected)
        victims.append(index.row());
    std::sort(victims.begin(), victims.end(), std::greater<int>());
    for (int row : victims)
        table->removeRow(row);
}

void ShellAliases::resetDefaults()
{
    setRows(defaults());
}

void ShellAliases::accept()
{
    const auto list = rows();

    QSettings settings;
    // the array is rewritten rather than updated, so a removed row does not
    // survive as a leftover entry past the new end
    settings.remove(Keys::ALIASES);
    settings.beginWriteArray(Keys::ALIASES, list.size());
    for (int i = 0; i < list.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(Keys::NAME, list[i].first);
        settings.setValue(Keys::VALUE, list[i].second);
    }
    settings.endArray();

    QDialog::accept();
}

// Local Variables:
// c-basic-offset: 4
// End:
