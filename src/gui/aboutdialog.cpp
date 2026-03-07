#include "aboutdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QTextBrowser>
#include <QSplitter>
#include <QListWidget>
#include <QFile>
#include <QCoreApplication>


AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(tr("About WavePSF"));
	setMinimumWidth(768);
	setMinimumHeight(256);

	QVBoxLayout* logoLayout = createLogoLayout();
	QTabWidget* tabWidget = new QTabWidget(this);
	setupTabs(tabWidget);

	QHBoxLayout* topLayout = new QHBoxLayout();
	topLayout->addLayout(logoLayout);
	topLayout->addWidget(tabWidget);

	QPushButton* closeButton = new QPushButton(tr("Close"));
	connect(closeButton, &QPushButton::clicked, this, &AboutDialog::close);

	QHBoxLayout* bottomLayout = new QHBoxLayout();
	bottomLayout->setContentsMargins(6, 6, 6, 6);
	bottomLayout->addStretch();
	bottomLayout->addWidget(closeButton);

	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	int margin = mainLayout->margin() + 10;
	mainLayout->setContentsMargins(margin, margin, margin, 0);
	mainLayout->setSpacing(mainLayout->spacing() + 10);
	mainLayout->addLayout(topLayout);
	mainLayout->addLayout(bottomLayout);
}

QVBoxLayout* AboutDialog::createLogoLayout()
{
	QVBoxLayout* layout = new QVBoxLayout();
	layout->setSpacing(0);
	layout->setContentsMargins(2, 15, 2, 0);

	QLabel* titleLabel = new QLabel(qApp->applicationName()+ "\n", this);
	QFont titleFont = titleLabel->font();
	titleFont.setBold(true);
	titleFont.setPointSize(titleFont.pointSize() + 4);
	titleLabel->setFont(titleFont);
	layout->addWidget(titleLabel, 0, Qt::AlignHCenter);

	QLabel* logoLabel = new QLabel(this);
	QPixmap pix(":/icons/wavepsf_psf_icon.png");
	logoLabel->setPixmap(pix.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	layout->addWidget(logoLabel, 0, Qt::AlignHCenter);

	layout->addSpacing(15);

	QLabel* authorLabel = new QLabel(this);
	authorLabel->setText("Author: Miroslav Zabic\n"
						 "Contact: zabic"
						 "@"
						 "spectralcode.de\n\n"
						 "Version: " + qApp->applicationVersion());
	authorLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	layout->addWidget(authorLabel);
	layout->addStretch();

	return layout;
}

void AboutDialog::setupTabs(QTabWidget* tabWidget)
{
	addAboutTab(tabWidget);
	addLicenseTab(tabWidget);
	addThirdPartyTab(tabWidget);
}

void AboutDialog::addAboutTab(QTabWidget* tabWidget)
{
	QLabel* label = new QLabel(this);
	label->setWordWrap(true);
	label->setTextFormat(Qt::RichText);
	label->setText(
		"<p>WavePSF is a free tool for point spread function estimation "
		"and deconvolution of hyperspectral imaging data.</p>"
		"<p>Based on: Zabic et al. &ldquo;Point spread function estimation "
		"with computed wavefronts for deconvolution of hyperspectral "
		"imaging data,&rdquo; <i>Scientific Reports</i> 15.1 (2025): 673. "
		"<a href='https://doi.org/10.1038/s41598-024-84790-6'>DOI</a></p>"
		"<p>License: GNU General Public License v3</p>"
		"<p>GitHub: <a href='https://github.com/spectralcode/WavePSF'>"
		"github.com/spectralcode/WavePSF</a></p>");
	label->setOpenExternalLinks(true);
	label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse);
	label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	label->setContentsMargins(10, 10, 10, 10);

	tabWidget->addTab(label, tr("About"));
}

void AboutDialog::addLicenseTab(QTabWidget* tabWidget)
{
	QString licenseText =
		"WavePSF is free software: you can redistribute it and/or modify "
		"it under the terms of the GNU General Public License as published by "
		"the Free Software Foundation, either version 3 of the License, or "
		"(at your option) any later version.<br>"
		"This program is distributed in the hope that it will be useful, "
		"but WITHOUT ANY WARRANTY; without even the implied warranty of "
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the "
		"GNU General Public License for more details.<br><br><hr><pre>";

	QFile licenseFile(":/LICENSE");
	if (licenseFile.exists() && licenseFile.open(QIODevice::ReadOnly)) {
		licenseText.append(QString::fromUtf8(licenseFile.readAll()));
		licenseFile.close();
	} else {
		licenseText.append("License file could not be opened.");
	}
	licenseText.append("</pre>");

	QTextEdit* textEdit = new QTextEdit(this);
	textEdit->setReadOnly(true);
	textEdit->setText(licenseText);
	tabWidget->addTab(textEdit, tr("License"));
}

void AboutDialog::addThirdPartyTab(QTabWidget* tabWidget)
{
	QWidget* widget = new QWidget(this);
	QHBoxLayout* layout = new QHBoxLayout(widget);
	layout->setContentsMargins(0, 0, 0, 0);

	QSplitter* splitter = new QSplitter(Qt::Horizontal, widget);
	layout->addWidget(splitter);

	QListWidget* componentList = new QListWidget(splitter);
	componentList->setMinimumWidth(100);
	componentList->setMaximumWidth(180);

	QTextBrowser* componentDetails = new QTextBrowser(splitter);
	componentDetails->setOpenExternalLinks(true);

	splitter->addWidget(componentList);
	splitter->addWidget(componentDetails);
	splitter->setStretchFactor(0, 1);
	splitter->setStretchFactor(1, 4);
	splitter->setSizes({100, 600});

	QList<ComponentInfo> components = getThirdPartyComponents();
	for (int i = 0; i < components.size(); ++i) {
		componentList->addItem(components.at(i).name);
	}

	connect(componentList, &QListWidget::currentTextChanged, this,
		[this, componentDetails, components](const QString& name) {
			for (int i = 0; i < components.size(); ++i) {
				if (components.at(i).name == name) {
					updateComponentDetails(componentDetails, components.at(i));
					break;
				}
			}
		}
	);

	if (componentList->count() > 0) {
		componentList->setCurrentRow(0);
	}

	tabWidget->addTab(widget, tr("Third-party components"));
}

QList<ComponentInfo> AboutDialog::getThirdPartyComponents()
{
	QList<ComponentInfo> components;

	ComponentInfo arrayfire;
	arrayfire.name = "ArrayFire";
	arrayfire.url = "https://arrayfire.com";
	arrayfire.licensePath = ":/aboutdata/thirdparty_licenses/arrayfire_license.txt";
	components.append(arrayfire);

	ComponentInfo libtiff;
	libtiff.name = "LibTIFF";
	libtiff.url = "https://libtiff.gitlab.io/libtiff/";
	libtiff.licensePath = ":/aboutdata/thirdparty_licenses/libtiff_license.txt";
	components.append(libtiff);

	ComponentInfo qcustomplot;
	qcustomplot.name = "QCustomPlot";
	qcustomplot.url = "https://www.qcustomplot.com";
	qcustomplot.licensePath = ":/aboutdata/thirdparty_licenses/qcustomplot_license.txt";
	components.append(qcustomplot);

	ComponentInfo qt;
	qt.name = "Qt Framework";
	qt.url = "https://www.qt.io";
	qt.licensePath = ":/aboutdata/thirdparty_licenses/qt_license.txt";
	components.append(qt);

	ComponentInfo lucide;
	lucide.name = "Lucide Icons";
	lucide.url = "https://lucide.dev";
	lucide.licensePath = ":/aboutdata/thirdparty_licenses/lucide_license.txt";
	components.append(lucide);

	return components;
}

QString AboutDialog::loadLicenseText(const QString& path)
{
	QFile file(path);
	if (file.exists() && file.open(QIODevice::ReadOnly)) {
		QString text = QString::fromUtf8(file.readAll());
		file.close();
		return text;
	}
	return tr("License file could not be opened: %1").arg(path);
}

void AboutDialog::updateComponentDetails(QTextBrowser* browser, const ComponentInfo& component)
{
	QString html = "<h2>" + component.name + "</h2>";
	if (!component.url.isEmpty()) {
		html += "<p><b>" + tr("Homepage") + ":</b> <a href='" +
				component.url + "'>" + component.url + "</a></p>";
	}
	html += "<h3>" + tr("License") + ":</h3><pre>";
	html += loadLicenseText(component.licensePath);
	html += "</pre>";
	browser->setHtml(html);
}
