// Unit tests for the PlotData column model and file parsers
// (src/plotdata.cpp), exercised without a GUI.

#include "plotdata.h"

#include "gtest/gtest.h"

#include <QByteArray>
#include <QDir>
#include <QString>
#include <QTemporaryFile>

namespace {

int colIndex(const PlotData &d, const QString &name)
{
    for (int i = 0; i < d.columnCount(); ++i)
        if (d.columnName(i) == name) return i;
    return -1;
}

TEST(PlotDataModel, AppendRowAndColumn)
{
    PlotData d;
    d.setColumnNames({"a", "b"});
    EXPECT_EQ(d.columnCount(), 2);
    EXPECT_EQ(d.rowCount(), 0);
    EXPECT_TRUE(d.isEmpty());

    EXPECT_TRUE(d.appendRow({1.0, 2.0}));
    EXPECT_TRUE(d.appendRow({3.0, 4.0}));
    EXPECT_FALSE(d.appendRow({5.0})); // wrong width
    EXPECT_EQ(d.rowCount(), 2);
    EXPECT_DOUBLE_EQ(d.column(0)[1], 3.0);
    EXPECT_DOUBLE_EQ(d.column(1)[0], 2.0);

    d.addColumn("c", {7.0, 8.0});
    EXPECT_EQ(d.columnCount(), 3);
    EXPECT_EQ(colIndex(d, "c"), 2);
}

TEST(PlotDataCsv, WithHeader)
{
    const QString text = "Step,Temp,Press\n0,300,1.0\n1,310,1.1\n";
    const PlotData d   = parsePlotCsv(text);
    ASSERT_EQ(d.columnCount(), 3);
    ASSERT_EQ(d.rowCount(), 2);
    EXPECT_EQ(d.columnName(0), "Step");
    EXPECT_EQ(d.columnName(1), "Temp");
    EXPECT_DOUBLE_EQ(d.column(1)[0], 300.0);
    EXPECT_DOUBLE_EQ(d.column(2)[1], 1.1);
}

TEST(PlotDataCsv, WithoutHeaderGeneratesNames)
{
    const QString text = "0,1,2\n3,4,5\n";
    const PlotData d   = parsePlotCsv(text);
    ASSERT_EQ(d.columnCount(), 3);
    ASSERT_EQ(d.rowCount(), 2);
    EXPECT_EQ(d.columnName(0), "column1");
    EXPECT_DOUBLE_EQ(d.column(0)[0], 0.0);
    EXPECT_DOUBLE_EQ(d.column(2)[1], 5.0);
}

TEST(PlotDataCsv, EmptyIsError)
{
    QString err;
    const PlotData d = parsePlotCsv("", &err);
    EXPECT_TRUE(d.isEmpty());
    EXPECT_FALSE(err.isEmpty());
}

TEST(PlotDataWhitespace, LammpsDatHeader)
{
    // mirrors ChartWindow::exportDat(): a description comment then a column
    // header comment, followed by aligned numeric columns
    const QString text = "# Thermodynamic data from foo\n"
                         "#          Step Temp Press\n"
                         "           0   300  1.0\n"
                         "           1   310  1.1\n";
    const PlotData d   = parsePlotWhitespace(text);
    ASSERT_EQ(d.columnCount(), 3);
    ASSERT_EQ(d.rowCount(), 2);
    EXPECT_EQ(d.columnName(0), "Step");
    EXPECT_EQ(d.columnName(2), "Press");
    EXPECT_DOUBLE_EQ(d.column(1)[1], 310.0);
}

TEST(PlotDataWhitespace, NoHeaderGeneratesNames)
{
    const QString text = "1.0 2.0\n3.0 4.0\n";
    const PlotData d   = parsePlotWhitespace(text);
    ASSERT_EQ(d.columnCount(), 2);
    ASSERT_EQ(d.rowCount(), 2);
    EXPECT_EQ(d.columnName(0), "column1");
    EXPECT_DOUBLE_EQ(d.column(1)[1], 4.0);
}

TEST(PlotDataYaml, KeywordsAndData)
{
    // mirrors ChartWindow::exportYaml()
    const QString text = "---\n"
                         "keywords: ['Step', Temp, Press]\n"
                         "data: \n"
                         "  - [0, 300, 1.0]\n"
                         "  - [1, 310, 1.1]\n"
                         "...\n";
    const PlotData d   = parsePlotYaml(text);
    ASSERT_EQ(d.columnCount(), 3);
    ASSERT_EQ(d.rowCount(), 2);
    EXPECT_EQ(d.columnName(0), "Step");
    EXPECT_EQ(d.columnName(1), "Temp");
    EXPECT_DOUBLE_EQ(d.column(2)[0], 1.0);
    EXPECT_DOUBLE_EQ(d.column(0)[1], 1.0);
}

TEST(PlotDataYaml, LammpsTrailingComma)
{
    // native LAMMPS thermo YAML writes a trailing comma before each closing
    // bracket; the parser must not treat the resulting empty field as data
    const QString text = "---\n"
                         "keywords: ['Step', 'Temp', 'Press', ]\n"
                         "data:\n"
                         "  - [0, 300, 1.0, ]\n"
                         "  - [50, 310, 1.1, ]\n"
                         "...\n";
    const PlotData d   = parsePlotYaml(text);
    ASSERT_EQ(d.columnCount(), 3);
    ASSERT_EQ(d.rowCount(), 2);
    EXPECT_EQ(d.columnName(0), "Step");
    EXPECT_EQ(d.columnName(2), "Press");
    EXPECT_DOUBLE_EQ(d.column(0)[1], 50.0);
    EXPECT_DOUBLE_EQ(d.column(2)[1], 1.1);
}

TEST(PlotDataYaml, IgnoresInterleavedLogLines)
{
    // a LAMMPS log can interleave other output (SHAKE/Bond/Angle stats, timing)
    // between the YAML thermo data rows; those lines must be ignored
    const QString text = "Per MPI rank memory allocation = 22 Mbytes\n"
                         "---\n"
                         "keywords: ['Step', 'Temp', ]\n"
                         "data:\n"
                         "  - [0, 300, ]\n"
                         "SHAKE stats (type/ave/delta/count) on step 100\n"
                         "Bond:    4   1.111     7.8e-07        9\n"
                         "Angle:  31   104.52    0.0005       640\n"
                         "  - [100, 310, ]\n"
                         "...\n"
                         "Loop time of 4.0 on 8 procs\n";
    const PlotData d   = parsePlotYaml(text);
    ASSERT_EQ(d.columnCount(), 2);
    ASSERT_EQ(d.rowCount(), 2);
    EXPECT_EQ(d.columnName(0), "Step");
    EXPECT_EQ(d.columnName(1), "Temp");
    EXPECT_DOUBLE_EQ(d.column(0)[1], 100.0);
    EXPECT_DOUBLE_EQ(d.column(1)[1], 310.0);
}

TEST(PlotDataYaml, SequenceOfMaps)
{
    // generic YAML sequence-of-maps format: - {key: val, key: val, ...}
    // column names come from the keys of the first entry
    const QString text = "# energy-volume curve\n"
                         "- {volume: 14.83, energy: -3.481}\n"
                         "- {volume: 15.12, energy: -3.507}\n"
                         "- {volume: 16.00, energy: -3.561}\n";
    const PlotData d   = parsePlotYaml(text);
    ASSERT_EQ(d.columnCount(), 2);
    ASSERT_EQ(d.rowCount(), 3);
    EXPECT_EQ(d.columnName(0), "volume");
    EXPECT_EQ(d.columnName(1), "energy");
    EXPECT_DOUBLE_EQ(d.column(0)[2], 16.00);
    EXPECT_DOUBLE_EQ(d.column(1)[0], -3.481);
}

TEST(LoadPlotData, DetectsYamlInLogByContent)
{
    // a .log extension is not an explicit format, so the YAML thermo block must
    // be detected from the file content
    QTemporaryFile f(QDir::tempPath() + "/plotdataXXXXXX.log");
    ASSERT_TRUE(f.open());
    f.write("LAMMPS run log preamble\n"
            "---\n"
            "keywords: ['Step', 'Temp', ]\n"
            "data:\n"
            "  - [0, 300, ]\n"
            "  - [50, 310, ]\n"
            "...\n"
            "Loop time of 4.0 on 8 procs\n");
    f.flush();

    QString err;
    const PlotData d = loadPlotData(f.fileName(), &err);
    EXPECT_TRUE(err.isEmpty()) << err.toStdString();
    ASSERT_EQ(d.columnCount(), 2);
    ASSERT_EQ(d.rowCount(), 2);
    EXPECT_EQ(d.columnName(1), "Temp");
    EXPECT_DOUBLE_EQ(d.column(0)[1], 50.0);
}

TEST(PlotDataJson, ArrayOfRows)
{
    const QByteArray json = "[[0,300,1.0],[1,310,1.1]]";
    const PlotData d      = parsePlotJson(json);
    ASSERT_EQ(d.columnCount(), 3);
    ASSERT_EQ(d.rowCount(), 2);
    EXPECT_EQ(d.columnName(0), "column1");
    EXPECT_DOUBLE_EQ(d.column(1)[1], 310.0);
}

TEST(PlotDataJson, ObjectOfArrays)
{
    const QByteArray json = R"({"Step":[0,1],"Temp":[300,310]})";
    const PlotData d      = parsePlotJson(json);
    ASSERT_EQ(d.columnCount(), 2);
    ASSERT_EQ(d.rowCount(), 2);
    const int s = colIndex(d, "Step");
    const int t = colIndex(d, "Temp");
    ASSERT_GE(s, 0);
    ASSERT_GE(t, 0);
    EXPECT_DOUBLE_EQ(d.column(s)[1], 1.0);
    EXPECT_DOUBLE_EQ(d.column(t)[0], 300.0);
}

TEST(PlotDataJson, UnequalColumnsIsError)
{
    QString err;
    const PlotData d = parsePlotJson(R"({"a":[1,2],"b":[3]})", &err);
    EXPECT_TRUE(d.isEmpty());
    EXPECT_FALSE(err.isEmpty());
}

TEST(PlotDataJson, MalformedIsError)
{
    QString err;
    const PlotData d = parsePlotJson("{not json", &err);
    EXPECT_TRUE(d.isEmpty());
    EXPECT_FALSE(err.isEmpty());
}

TEST(LoadPlotData, DispatchesByExtension)
{
    QTemporaryFile f(QDir::tempPath() + "/plotdataXXXXXX.csv");
    ASSERT_TRUE(f.open());
    f.write("Step,Val\n0,1.5\n1,2.5\n");
    f.flush();
    const QString name = f.fileName();

    QString err;
    const PlotData d = loadPlotData(name, &err);
    EXPECT_TRUE(err.isEmpty()) << err.toStdString();
    ASSERT_EQ(d.columnCount(), 2);
    ASSERT_EQ(d.rowCount(), 2);
    EXPECT_EQ(d.columnName(1), "Val");
    EXPECT_DOUBLE_EQ(d.column(1)[1], 2.5);
}

// ---- export writers (round-trip through the matching parser) -------------

PlotData sampleTable()
{
    PlotData d;
    d.setColumnNames({"Step", "Temp", "Press"});
    d.appendRow({0.0, 300.0, 1.5});
    d.appendRow({50.0, 310.0, 1.1});
    return d;
}

TEST(PlotDataExport, CsvRoundTrip)
{
    const PlotData back = parsePlotCsv(writePlotCsv(sampleTable()));
    ASSERT_EQ(back.columnCount(), 3);
    ASSERT_EQ(back.rowCount(), 2);
    EXPECT_EQ(back.columnName(0), "Step");
    EXPECT_EQ(back.columnName(2), "Press");
    EXPECT_NEAR(back.column(1)[0], 300.0, 1.0e-9);
    EXPECT_NEAR(back.column(2)[1], 1.1, 1.0e-9);
}

TEST(PlotDataExport, DatRoundTrip)
{
    const PlotData back = parsePlotWhitespace(writePlotDat(sampleTable(), "unit-test"));
    ASSERT_EQ(back.columnCount(), 3);
    ASSERT_EQ(back.rowCount(), 2);
    EXPECT_EQ(back.columnName(0), "Step");
    EXPECT_EQ(back.columnName(2), "Press");
    EXPECT_NEAR(back.column(0)[1], 50.0, 1.0e-9);
    EXPECT_NEAR(back.column(1)[1], 310.0, 1.0e-9);
}

TEST(PlotDataExport, YamlRoundTripAndQuoting)
{
    const QString yaml = writePlotYaml(sampleTable());
    // headers must be quoted strings
    EXPECT_TRUE(yaml.contains("keywords: ['Step', 'Temp', 'Press']")) << yaml.toStdString();

    const PlotData back = parsePlotYaml(yaml);
    ASSERT_EQ(back.columnCount(), 3);
    ASSERT_EQ(back.rowCount(), 2);
    EXPECT_EQ(back.columnName(1), "Temp");
    EXPECT_NEAR(back.column(2)[0], 1.5, 1.0e-9);
    EXPECT_NEAR(back.column(0)[1], 50.0, 1.0e-9);
}

TEST(PlotDataExport, YamlQuotesNamesWithFlowChars)
{
    PlotData d;
    d.setColumnNames({"Step", "c_rdf[1]"});
    d.appendRow({0.0, 2.5});
    const PlotData back = parsePlotYaml(writePlotYaml(d));
    ASSERT_EQ(back.columnCount(), 2);
    EXPECT_EQ(back.columnName(1), "c_rdf[1]");
}

} // namespace
