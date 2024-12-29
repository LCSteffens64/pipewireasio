#include "dialog.hpp"
#include "dialog.moc"
#include "../pw_helper.hpp"

#include <QtCore/qglobal.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qbuttongroup.h>

#include <pipewire/keys.h>

PwAsioDialog::PwAsioDialog(PwHelper::Helper *helper)
	: layoutGroup(new QButtonGroup)
	, pw_helper(helper)
{
	ui.setupUi(this);
	layoutGroup->addButton(ui.layoutButton_0, 0);
	layoutGroup->addButton(ui.layoutButton_1, 1);
	connect(
		layoutGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
		this, QOverload<QAbstractButton *>::of(&PwAsioDialog::layoutButtonClicked));

	refreshDevices();
}

PwAsioDialog::~PwAsioDialog() {
	delete layoutGroup;
}

void PwAsioDialog::layoutButtonClicked([[maybe_unused]] QAbstractButton *button) {
	//int bid = layoutGroup->id(button);
	//printf("[DBG] Button clicked: %d\n", bid);
	auto *checked = layoutGroup->checkedButton();
	int cid = layoutGroup->id(checked);
	//printf("[DBG] Currently checked: %d\n", cid);
	ui.io_config->setCurrentIndex(cid);
}

void PwAsioDialog::refreshDevices() {
	auto nodes = PwHelper::enumerate_pipewire_endpoints(pw_helper);

	ui.input_devices->clear();
	ui.output_devices->clear();

	ui.input_devices->addItem("<default>");
	ui.output_devices->addItem("<default>");

	for (auto* node: nodes) {
		std::string s_name, s_descr;
		std::string media_class;
		std::pair<std::string_view, std::string*> props[] {
			{PW_KEY_NODE_NAME, &s_name},
			{PW_KEY_NODE_DESCRIPTION, &s_descr},
			{PW_KEY_MEDIA_CLASS, &media_class},
		};
		PwHelper::get_node_props(pw_helper, node, props);

		using namespace std::string_view_literals;

		if (s_descr.empty())
			s_descr = std::move(s_name);

		if (media_class == "Audio/Source"sv) {
			ui.input_devices->addItem(QString::fromStdString(s_descr));
		} else if (media_class == "Audio/Sink"sv) {
			ui.output_devices->addItem(QString::fromStdString(s_descr));
		}
	}
}
