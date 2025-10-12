#include "stylemanager.h"
#include "utils/settingsfilemanager.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QPalette>
#include <QStyle>
#include <QWidget>
#include <QGraphicsView>
#include <QTimer>

#include "imagesessionviewer/graphicsview.h"

namespace {
	const char* SETTINGS_GROUP = "StyleManager";
	const char* STYLE_MODE_KEY = "styleMode";
}

StyleManager::StyleManager(QApplication* app, SettingsFileManager* guiSettings, QObject *parent)
	: QObject(parent), application(app), guiSettings(guiSettings), currentStyle(System) {

	this->initializeStyleMap();

	this->loadSettings();
	this->applyStyle(this->currentStyle);
}

StyleManager::~StyleManager() {
	this->saveSettings();
}

void StyleManager::initializeStyleMap() {
	//auto-generated style map from STYLE_LIST macro
	#define STYLE_ITEM(enumName, displayName, path, qtStyle) \
		this->styleInfoMap[enumName] = {displayName, path, qtStyle};
	STYLE_LIST
	#undef STYLE_ITEM
}

void StyleManager::setStyleMode(StyleMode mode) {
	if (this->currentStyle != mode) {
		this->currentStyle = mode;
		this->saveSettings();
		this->applyStyle(mode);
		emit styleChanged(mode);
	}
}

StyleManager::StyleMode StyleManager::getStyleMode() const {
	return this->currentStyle;
}

void StyleManager::applyStyle(StyleMode mode) {
	if (!this->styleInfoMap.contains(mode)) {
		qWarning() << "Unknown style mode:" << mode << "- falling back to System style";
		mode = System;
	}

	const StyleInfo& info = this->styleInfoMap[mode];

	//clear any previous stylesheet to avoid stale rules bleeding over
	this->application->setStyleSheet(QString());

	//set Qt style first
	if (!info.qtStyle.isEmpty()) {
		this->application->setStyle(info.qtStyle);
	}

	//apply palette for dark fusion; reset for others
	if (mode == DarkFusion) {
		this->applyDarkFusionPalette(this->application);
	} else {
		this->application->setPalette(this->application->style()->standardPalette());
	}

	//apply stylesheet
	if (info.stylesheetPath.isEmpty()) {
		this->application->setStyleSheet("");
		this->refreshAllWidgets();
	} else {
		QString stylesheet = this->loadStyleSheet(info.stylesheetPath);
		if (stylesheet.isEmpty()) {
			qWarning() << "Could not load stylesheet:" << info.stylesheetPath << "- falling back to system style";
			this->refreshAllWidgets();
			this->application->setStyleSheet("");
		} else {
			this->refreshAllWidgets();
			this->application->setStyleSheet(stylesheet);
		}
	}
	QTimer::singleShot(0, this, &StyleManager::refreshAllWidgets); //queue refresh for next event loop iteration. not sure why, but this is needed since otherwise GraphicsView does not update the style properly
}

QString StyleManager::getStyleName(StyleMode mode) const {
	return this->styleInfoMap.value(mode, {"Unknown", "", ""}).name;
}

const QMap<StyleManager::StyleMode, StyleManager::StyleInfo>& StyleManager::getStyleInfoMap() const {
	return this->styleInfoMap;
}

void StyleManager::loadSettings() {
	QVariantMap settings = this->guiSettings->getStoredSettings(SETTINGS_GROUP);

	int styleInt = settings.value(STYLE_MODE_KEY, static_cast<int>(System)).toInt();
	this->currentStyle = static_cast<StyleMode>(styleInt);
}

void StyleManager::saveSettings() {
	QVariantMap settings;
	settings[STYLE_MODE_KEY] = static_cast<int>(this->currentStyle);

	this->guiSettings->storeSettings(SETTINGS_GROUP, settings);
}

QString StyleManager::loadStyleSheet(const QString& fileName) {
	QFile file(fileName);
	if (file.open(QFile::ReadOnly | QFile::Text)) {
		QTextStream stream(&file);
		return stream.readAll();
	} else {
		qDebug() << "Could not load stylesheet:" << fileName;
		return QString();
	}
}

void StyleManager::refreshAllWidgets() {
	QPalette newPalette = this->application->palette();
	QWidgetList widgets = this->application->allWidgets();
	for (QWidget* widget : qAsConst(widgets)) {
		widget->setPalette(newPalette);
		widget->style()->unpolish(widget);
		widget->setStyleSheet("");
		widget->style()->polish(widget);

		// Special handling for GraphicsView before general update
		GraphicsView* graphicsView = qobject_cast<GraphicsView*>(widget);
		if (graphicsView) {
			graphicsView->refreshForStyleChange();
		} else {
			widget->update();
			widget->repaint();
		}
	}

	this->application->processEvents();;
}

void StyleManager::applyDarkFusionPalette(QApplication* app) {
	QPalette dark;
	dark.setColor(QPalette::Text, QColor(255, 255, 255));
	dark.setColor(QPalette::WindowText, QColor(255, 255, 255));
	dark.setColor(QPalette::Window, QColor(50, 50, 50));
	dark.setColor(QPalette::Button, QColor(50, 50, 50));
	dark.setColor(QPalette::Base, QColor(25, 25, 25));
	dark.setColor(QPalette::AlternateBase, QColor(50, 50, 50));
	dark.setColor(QPalette::ToolTipBase, QColor(50, 50, 50));
	dark.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
	dark.setColor(QPalette::ButtonText, QColor(255, 255, 255));
	dark.setColor(QPalette::BrightText, QColor(255, 50, 50));
	dark.setColor(QPalette::Link, QColor(40, 130, 220));
	dark.setColor(QPalette::Highlight, QColor(40, 130, 220));
	dark.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
	dark.setColor(QPalette::Disabled, QPalette::Text, QColor(99, 99, 99));
	dark.setColor(QPalette::Disabled, QPalette::WindowText, QColor(99, 99, 99));
	dark.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(99, 99, 99));
	dark.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
	dark.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(99, 99, 99));
	app->setPalette(dark);
}
