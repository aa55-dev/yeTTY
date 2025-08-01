#include "aboutdialog.hpp"
#include "ui_aboutdialog.h"
#include "yetty.version.h"

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    const auto pm = QApplication::windowIcon().pixmap(QSize { 64, 64 });
    ui->appLogoLabel->setPixmap(pm);
    ui->appTitleLabel->setTextFormat(Qt::TextFormat::RichText);
    ui->appTitleLabel->setText(QStringLiteral("<b>") + QStringLiteral(PROJECT_NAME) + QStringLiteral("</b>")
        + "<br>" + "version: " + QStringLiteral(PROJECT_VERSION));

    ui->appUrlLabel->setText(QStringLiteral("Homepage: <a href=\"https://yetty.aa55.dev/\">https://yetty.aa55.dev/</a><br>"
                                            "Report bugs: <a href=\"https://github.com/aa55-dev/yeTTY/issues\">https://github.com/aa55-dev/yeTTY/issues</a>"));
    ui->appUrlLabel->setTextFormat(Qt::RichText);
    ui->appUrlLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->appUrlLabel->setOpenExternalLinks(true);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
