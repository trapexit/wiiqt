#ifndef TEXTDIALOG_H
#define TEXTDIALOG_H

#include <QDialog>

namespace Ui {
    class TextDialog;
}

//generic dialog to get text
class TextDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TextDialog(QWidget *parent = 0, const QString &txt = QString() );
    ~TextDialog();

    static QString GetText( QWidget *parent = 0, const QString &txt = QString() );

private:
    Ui::TextDialog *ui;
    QString ret;

private slots:
    void on_buttonBox_accepted();
};

#endif // TEXTDIALOG_H
