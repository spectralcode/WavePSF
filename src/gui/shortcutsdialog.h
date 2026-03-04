#ifndef SHORTCUTSDIALOG_H
#define SHORTCUTSDIALOG_H

#include <QDialog>

class ShortcutsDialog : public QDialog
{
public:
	explicit ShortcutsDialog(QWidget* parent = nullptr);

private:
	static QString buildHtml(const QString& keyColor);
};

#endif // SHORTCUTSDIALOG_H
