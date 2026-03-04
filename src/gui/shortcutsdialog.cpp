#include "shortcutsdialog.h"

#include <QTextBrowser>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPalette>

ShortcutsDialog::ShortcutsDialog(QWidget* parent) : QDialog(parent)
{
	this->setWindowTitle(tr("Keyboard Shortcuts"));
	this->resize(520, 580);

	QVBoxLayout* layout = new QVBoxLayout(this);

	QTextBrowser* browser = new QTextBrowser(this);
	browser->setOpenExternalLinks(false);
	const QString keyColor = this->palette().color(QPalette::Link).name();
	browser->setHtml(buildHtml(keyColor));
	layout->addWidget(browser);

	QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
	layout->addWidget(buttons);
}

QString ShortcutsDialog::buildHtml(const QString& keyColor)
{
	// CSS shared across all tables
	const QString css =
		QStringLiteral("<style>"
		"body { font-family: sans-serif; font-size: 13px; }"
		"h3 { margin-top: 14px; margin-bottom: 4px; border-bottom: 1px solid #aaa; padding-bottom: 2px; }"
		"table { width: 100%; border-collapse: collapse; margin-bottom: 6px; }"
		"td { padding: 3px 6px; vertical-align: top; }"
		"tr:nth-child(even) { background-color: #f4f4f4; }"
		"</style>") +
		QStringLiteral("<style>td.key { font-family: monospace; white-space: nowrap; color: ") +
		keyColor +
		QStringLiteral("; width: 40%; }</style>");

	// Helper lambda for a section header
	auto section = [](const QString& title) {
		return QStringLiteral("<h3>") + title + QStringLiteral("</h3><table>");
	};

	// Helper lambda for a shortcut row
	auto row = [](const QString& key, const QString& description) {
		return QStringLiteral("<tr><td class='key'>") + key +
		       QStringLiteral("</td><td>") + description + QStringLiteral("</td></tr>");
	};

	QString html = css;

	// --- File Operations ---
	html += section(tr("File Operations"));
	html += row(tr("Ctrl+O"),          tr("Open image data"));
	html += row(tr("Ctrl+G"),          tr("Open ground truth image"));
	html += row(tr("Ctrl+S"),          tr("Save parameters (CSV)"));
	html += row(tr("Ctrl+Shift+S"),    tr("Save output data"));
	html += row(tr("Ctrl+Q"),          tr("Exit"));
	html += QStringLiteral("</table>");

	// --- Processing ---
	html += section(tr("Processing"));
	html += row(tr("Ctrl+Shift+D"),    tr("Deconvolve all frames (only if parameter csv is loaded)"));
	html += QStringLiteral("</table>");

	// --- View ---
	html += section(tr("View"));
	html += row(tr("F12"),             tr("Show / hide message console"));
	html += QStringLiteral("</table>");

	// --- Input/Output Viewer ---
	html += section(tr("Input/Output Viewer (click viewer to activate)"));
	html += row(tr("Hold X"),          tr("Preview ground truth image while held"));
	html += row(tr("R"),               tr("Rotate view 90&deg; clockwise"));
	html += row(tr("H"),               tr("Flip image horizontal"));
	html += row(tr("V"),               tr("Flip image vertical"));
	html += row(tr("+"),               tr("Zoom in"));
	html += row(tr("-"),               tr("Zoom out"));
	html += row(tr("Ctrl + Scroll"),   tr("Zoom centered on cursor"));
	html += row(tr("Right-click"),     tr("Context menu (rotate, flip, zoom, copy/paste/reset)"));
	html += QStringLiteral("</table>");

	// --- PSF Coefficients ---
	html += section(tr("PSF Coefficients (global, outside text fields)"));
	html += row(tr("Ctrl+C"),          tr("Copy coefficients of active patch"));
	html += row(tr("Ctrl+V"),          tr("Paste coefficients to active patch"));
	html += row(tr("Delete"),          tr("Reset coefficients of active patch to zero"));
	html += QStringLiteral("</table>");

	// --- Wavefront Plot ---
	html += section(tr("Wavefront Plot (click plot to activate)"));
	html += row(tr("Scroll"),          tr("Zoom in / out"));
	html += row(tr("Left-drag"),       tr("Pan"));
	html += row(tr("Double-click"),    tr("Reset view"));
	html += row(tr("Right-click"),     tr("Context menu (save, auto-scale, color map, grid...)"));
	html += QStringLiteral("</table>");

	// --- PSF Preview ---
	html += section(tr("PSF Preview (click preview to activate)"));
	html += row(tr("Scroll"),          tr("Zoom in / out toward cursor"));
	html += row(tr("Left-drag"),       tr("Pan"));
	html += row(tr("Double-click"),    tr("Fit to view"));
	html += row(tr("Right-click"),     tr("Save image..."));
	html += QStringLiteral("</table>");

	// --- Optimization Metric Plot ---
	html += section(tr("Optimization Metric Plot (click plot to activate)"));
	html += row(tr("Scroll"),          tr("Zoom in / out"));
	html += row(tr("Left-drag"),       tr("Pan"));
	html += row(tr("Double-click"),    tr("Reset view"));
	html += row(tr("Right-click"),     tr("Context menu (save, reset view)"));
	html += QStringLiteral("</table>");

	return html;
}
