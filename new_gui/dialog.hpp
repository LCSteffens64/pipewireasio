#pragma once

#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QAbstractButton>

#include "ui_dialog.hpp"
#include "../pw_helper.hpp"

class PwAsioDialog : public QDialog {
	Q_OBJECT

	public:
	PwAsioDialog(PwHelper::Helper *pw_helper);
	~PwAsioDialog();

	private:
	void layoutButtonClicked(QAbstractButton *button);

	void refreshDevices();

	Ui::PwAsioDialog ui;
	QButtonGroup *layoutGroup;

	PwHelper::Helper *pw_helper;
};
