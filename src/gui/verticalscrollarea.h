#ifndef VERTICALSCROLLAREA_H
#define VERTICALSCROLLAREA_H

#include <QScrollArea>

class VerticalScrollArea : public QScrollArea
{
public:
	explicit VerticalScrollArea(QWidget* parent = nullptr);

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	bool event(QEvent* e) override;
};

#endif // VERTICALSCROLLAREA_H
