#include "gui.h"
#include "dialog.hpp"

#include <cassert>
#include <atomic>
#include <thread>

#include <QtWidgets/QApplication>

enum class GuiState {
	Init,
	Running,
	Stopping,
	Stopped,
};

struct pwasio_gui {
	struct pwasio_gui_conf *conf;
	PwAsioDialog *dialog;
	std::thread thread;
	std::atomic<GuiState> state;

	pwasio_gui(struct pwasio_gui_conf *conf)
		: conf(conf)
		, dialog(nullptr)
		, thread()
		, state(GuiState::Init)
	{}

	~pwasio_gui() {
		delete dialog;
	}

	void init() {
		dialog = new PwAsioDialog(reinterpret_cast<PwHelper::Helper *>(conf->pw_helper));
		QObject::connect(
			dialog, QOverload<int>::of(&QDialog::finished),
			[this] (int status) {
				puts("WINDOW CLOSED");
				if (status == QDialog::Accepted) {
					this->apply_config();
				}
				this->conf->closed(this->conf);
			});
		load_config();
		dialog->showNormal();
	}

	void load_config() {
		conf->load_config(conf);
		dialog->setBufferSize(conf->cf_buffer_size);
	}

	void apply_config() {
		conf->cf_buffer_size = dialog->getBufferSize();
		conf->apply_config(conf);
	}
};

static void run_gui(struct pwasio_gui *gui) {
	gui->state.wait(GuiState::Init);
	gui->thread.detach();
	int argc = 0;
	QApplication app(argc, nullptr);
	app.setApplicationName("PipeWire ASIO Settings");
	gui->init();
	QApplication::exec();
	if (gui->state.exchange(GuiState::Stopped) == GuiState::Stopping) {
		delete gui;
	}
}

extern "C" {

struct pwasio_gui *pwasio_init_gui(struct pwasio_gui_conf *conf) {
	auto *gui = new struct pwasio_gui(conf);
	gui->thread = std::thread(run_gui, gui);
	gui->state.store(GuiState::Running);
	gui->state.notify_one();
	return gui;
}

void pwasio_destroy_gui(struct pwasio_gui *gui) {
	GuiState prev_state = GuiState::Running;
	if (!gui->state.compare_exchange_strong(prev_state, GuiState::Stopping)) {
		assert(gui->state.load() == GuiState::Stopped);
		delete gui;
	}
}

}
