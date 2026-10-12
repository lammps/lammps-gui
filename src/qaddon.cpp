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

#include "qaddon.h"

#include <QPainter>
#include <QString>
#include <QStringList>
#include <QWidget>

namespace {
// clang-format off
const QStringList imagecolors = {
    "aliceblue", "antiquewhite", "aqua", "aquamarine", "azure", "beige", "bisque", "black",
    "blanchedalmond", "blue", "blueviolet", "brown", "burlywood", "cadetblue", "chartreuse",
    "chocolate", "coral", "cornflowerblue", "cornsilk", "crimson", "cyan", "darkblue", "darkcyan",
    "darkgoldenrod", "darkgray", "darkgreen", "darkkhaki", "darkmagenta", "darkolivegreen",
    "darkorange", "darkorchid", "darkred", "darksalmon", "darkseagreen", "darkslateblue",
    "darkslategray", "darkturquoise", "darkviolet", "deeppink", "deepskyblue", "dimgray",
    "dodgerblue", "firebrick", "floralwhite", "forestgreen", "fuchsia", "gainsboro", "ghostwhite",
    "gold", "goldenrod", "gray", "green", "greenyellow", "honeydew", "hotpink", "indianred",
    "indigo", "ivory", "khaki", "lavender", "lavenderblush", "lawngreen", "lemonchiffon",
    "lightblue", "lightcoral", "lightcyan", "lightgoldenrodyellow", "lightgreen", "lightgrey",
    "lightpink", "lightsalmon", "lightseagreen", "lightskyblue", "lightslategray", "lightsteelblue",
    "lightyellow", "lime", "limegreen", "linen", "magenta", "maroon", "mediumaquamarine",
    "mediumblue", "mediumorchid", "mediumpurple", "mediumseagreen", "mediumslateblue",
    "mediumspringgreen", "mediumturquoise", "mediumvioletred", "midnightblue", "mintcream",
    "mistyrose", "moccasin", "navajowhite", "navy", "oldlace", "olive", "olivedrab", "orange",
    "orangered", "orchid", "palegoldenrod", "palegreen", "paleturquoise", "palevioletred",
    "papayawhip", "peachpuff", "peru", "pink", "plum", "powderblue", "purple", "red", "rosybrown",
    "royalblue", "saddlebrown", "salmon", "sandybrown", "seagreen", "seashell", "sienna", "silver",
    "skyblue", "slateblue", "slategray", "snow", "springgreen", "steelblue", "tan", "teal",
    "thistle", "tomato", "turquoise", "violet", "wheat", "white", "whitesmoke", "yellow",
    "yellowgreen"
};
// clang-format on
} // namespace

const QStringList &lammpsImageColors()
{
    return imagecolors;
}

QHline::QHline(QWidget *parent) : QFrame(parent)
{
    setGeometry(QRect(0, 0, 100, 3));
    setFrameShape(QFrame::HLine);
    setFrameShadow(QFrame::Sunken);
}

QColorValidator::QColorValidator(QWidget *parent) : QValidator(parent) {}

void QColorValidator::fixup(QString &input) const
{
    // remove leading/trailing whitespace and make lowercase
    input = input.trimmed();
    input = input.toLower();
}

QValidator::State QColorValidator::validate(QString &input, int &) const
{
    QString match;

    // find if input string is contained in list of colors
    for (const auto &color : imagecolors) {
        if (color.startsWith(input)) {
            match = color;
            break;
        }
    }

    if (match == input) {
        return QValidator::Acceptable;
    } else if (match.size() > 0) {
        return QValidator::Intermediate;
    }
    return QValidator::Invalid;
}

// complete color inputs
QColorCompleter::QColorCompleter(QWidget *parent) : QCompleter(imagecolors, parent)
{
    setCompletionMode(QCompleter::InlineCompletion);
    setModelSorting(QCompleter::CaseInsensitivelySortedModel);
};

/* -------------------------------------------------------------------- */

VerticalLabel::VerticalLabel(const QString &text, QWidget *parent) : QWidget(parent), m_text(text)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void VerticalLabel::setText(const QString &text)
{
    m_text = text;
    updateGeometry();
    update();
}

QString VerticalLabel::text() const
{
    return m_text;
}

void VerticalLabel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(palette().color(QPalette::WindowText));
    painter.setFont(font());
    painter.translate(0, height());
    painter.rotate(-90);
    QMargins m = contentsMargins();
    // In rotated coords (after translate+rotate-90): painter_x = widget_bottom-to-top,
    // painter_y = widget_left-to-right. So margins map: bottom→x_origin, left→y_origin.
    painter.drawText(QRect(m.bottom(), m.left(), height() - m.top() - m.bottom(),
                           width() - m.left() - m.right()),
                     Qt::AlignCenter, m_text);
}

QSize VerticalLabel::sizeHint() const
{
    QFontMetrics fm(font());
    QMargins m = contentsMargins();
    return QSize(fm.height() + 4 + m.left() + m.right(),
                 fm.horizontalAdvance(m_text) + 4 + m.top() + m.bottom());
}

QSize VerticalLabel::minimumSizeHint() const
{
    return sizeHint();
}

// Local Variables:
// c-basic-offset: 4
// End:
