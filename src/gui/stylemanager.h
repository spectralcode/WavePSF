#ifndef STYLEMANAGER_H
#define STYLEMANAGER_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QMap>

class QApplication;
class SettingsFileManager;

class StyleManager : public QObject {
	Q_OBJECT

public:
	//single place to define all styles
	#define STYLE_LIST \
		STYLE_ITEM(System, "Light Fusion", "", "fusion") \
		STYLE_ITEM(DarkFusion, "Dark Fusion", "", "fusion") \
		STYLE_ITEM(Windows, "Windows Classic", "", "windows") \
		STYLE_ITEM(WindowsVista, "Windows Vista", "", "windowsvista") \
		STYLE_ITEM(Light, "Light", ":/styles/light.qss", "fusion") \
		STYLE_ITEM(Dark, "Dark", ":/styles/dark.qss", "fusion") \
		//STYLE_ITEM(HighContrast, "High Contrast", ":/styles/highcontrast.qss", "fusion") \

	//auto-generated enum from the style list
	enum StyleMode {
		#define STYLE_ITEM(enumName, displayName, path, qtStyle) enumName,
		STYLE_LIST
		#undef STYLE_ITEM
	};

	struct StyleInfo {
		QString name;
		QString stylesheetPath; //path to .qss file, empty for system style
		QString qtStyle; //"fusion", "windows", etc.
	};

	explicit StyleManager(QApplication* app, SettingsFileManager* guiSettings, QObject *parent = nullptr);
	~StyleManager();

	void setStyleMode(StyleMode mode);
	StyleMode getStyleMode() const;

	// Utility methods
	QString getStyleName(StyleMode mode) const;
	const QMap<StyleMode, StyleInfo>& getStyleInfoMap() const;

signals:
	void styleChanged(StyleManager::StyleMode newStyle);

private:
	void initializeStyleMap();
	void applyStyle(StyleMode mode);
	void loadSettings();
	void saveSettings();
	QString loadStyleSheet(const QString& fileName);
	void refreshAllWidgets();
	void applyDarkFusionPalette(QApplication* app);

	QApplication* application;
	SettingsFileManager* guiSettings;
	StyleMode currentStyle;

	QMap<StyleMode, StyleInfo> styleInfoMap;
};

#endif // STYLEMANAGER_H
