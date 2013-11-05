//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#include "reportuser.hpp"
#include "ui_reportuser.h"
using namespace Huggle;

ReportUser::ReportUser(QWidget *parent) : QDialog(parent), ui(new Ui::ReportUser)
{
    ui->setupUi(this);
    this->user = NULL;
    this->q = NULL;
    ui->tableWidget->horizontalHeader()->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->pushButton->setEnabled(false);
    /// \todo LOCALIZE ME
    ui->pushButton->setText("Retrieving history...");
    QStringList header;
    ui->tableWidget->setColumnCount(5);
    this->diff = new QTimer(this);
    connect(this->diff, SIGNAL(timeout()), this, SLOT(On_DiffTick()));
    /// \todo LOCALIZE ME
    header << "Page" << "Time" << "Link" << "DiffID" << "Include in report";
    ui->tableWidget->setHorizontalHeaderLabels(header);
    this->tq = NULL;
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    Messaging = false;
    this->qd = NULL;
    report = "";
    Loading = false;
#if QT_VERSION >= 0x050000
// Qt5 code
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
// Qt4 code
    ui->tableWidget->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#endif
    //ui->tableWidget->horizontalHeaderItem(0)->setSizeHint(QSize(20,-1));
    ui->tableWidget->setShowGrid(false);
    this->t2 = NULL;
    this->timer = NULL;
    /// \todo LOCALIZE ME
    ui->webView->setHtml("Please select a diff in list in order to open preview");
}

bool ReportUser::SetUser(WikiUser *u)
{
    if (q != NULL)
    {
        delete q;
        q = NULL;
    }
    this->user = u;
    ui->label->setText(u->Username);
    q = new ApiQuery();
    q->Parameters = "list=recentchanges&rcuser=" + QUrl::toPercentEncoding(u->Username) +
            "&rcprop=user%7Ccomment%7Ctimestamp%7Ctitle%7Cids%7Csizes&rclimit=20&rctype=edit%7Cnew";
    q->SetAction(ActionQuery);
    q->Process();
    this->timer = new QTimer(this);
    connect(this->timer, SIGNAL(timeout()), this, SLOT(Tick()));
    this->timer->start(200);
    return true;
}

ReportUser::~ReportUser()
{
    if (this->q != NULL)
    {
        this->q->SafeDelete();
    }
    if (this->qd != NULL)
    {
        this->qd->SafeDelete();
    }
    delete this->diff;
    if (this->tq != NULL)
    {
        this->tq->SafeDelete();
    }
    delete this->ui;
}

void ReportUser::Tick()
{
    if (this->q == NULL)
    {
        return;
    }

    if (Loading)
    {
        if (q->Processed())
        {
            QDomDocument d;
            d.setContent(q->Result->Data);
            QDomNodeList results = d.elementsByTagName("rev");
            if (results.count() == 0)
            {
                /// \todo LOCALIZE ME
                ui->pushButton->setText("Error unable to retrieve report page at " + Configuration::LocalConfig_ReportPath);
                this->timer->stop();
                return;
            }
            QDomElement e = results.at(0).toElement();
            _p = e.text();
            if (!this->CheckUser())
            {
                /// \todo LOCALIZE ME
                ui->pushButton->setText("This user is already reported");
                this->timer->stop();
                return;
            }
            this->InsertUser();
            Core::EditPage(Core::AIVP, _p, "Reporting " + user->Username);
            this->timer->stop();
            this->user->IsReported = true;
            WikiUser::UpdateUser(this->user);
            /// \todo LOCALIZE ME
            ui->pushButton->setText("Reported");
            return;
        }
        return;
    }

    if (q->Processed())
    {
        Core::DebugLog(q->Result->Data);
        QDomDocument d;
        d.setContent(q->Result->Data);
        QDomNodeList results = d.elementsByTagName("rc");
        int xx = 0;
        if (results.count() > 0)
        {
            while (results.count() > xx)
            {
                QDomElement edit = results.at(xx).toElement();
                if (!edit.attributes().contains("type"))
                {
                    continue;
                }
                QString page = "unknown page";
                if (edit.attributes().contains("title"))
                {
                    page = edit.attribute("title");
                }
                QString time = "unknown time";
                if (edit.attributes().contains("timestamp"))
                {
                    time = edit.attribute("timestamp");
                }
                QString diff = "";
                if (edit.attributes().contains("revid"))
                {
                    diff = edit.attribute("revid");
                }
                QString link = Core::GetProjectScriptURL() + "index.php?title=" + page + "&diff=" + diff;
                ui->tableWidget->insertRow(0);
                ui->tableWidget->setItem(0, 0, new QTableWidgetItem(page));
                ui->tableWidget->setItem(0, 1, new QTableWidgetItem(time));
                ui->tableWidget->setItem(0, 2, new QTableWidgetItem(link));
                ui->tableWidget->setItem(0, 3, new QTableWidgetItem(diff));
                QCheckBox *Item = new QCheckBox(this);
                this->CheckBoxes.insert(0, Item);
                ui->tableWidget->setCellWidget(0, 4, Item);
                xx++;
            }
        }
        this->timer->stop();
        ui->pushButton->setEnabled(true);
        ui->pushButton->setText("Report");
    }
}

void ReportUser::On_DiffTick()
{
    if (this->qd == NULL)
    {
        return;
    }
    if (!this->qd->Processed())
    {
        return;
    }
    if (this->qd->Result->Failed)
    {
        /// \todo LOCALIZE ME
        ui->webView->setHtml("Unable to retrieve diff: " + this->qd->Result->ErrorMessage);
        this->diff->stop();
        return;
    }

    QString Summary;
    QString Diff;

    QDomDocument d;
    d.setContent(this->qd->Result->Data);
    QDomNodeList l = d.elementsByTagName("rev");
    QDomNodeList diff = d.elementsByTagName("diff");
    if (diff.count() > 0)
    {
        QDomElement e = diff.at(0).toElement();
        if (e.nodeName() == "diff")
        {
            Diff = e.text();
        }
    } else
    {
        Core::DebugLog(this->qd->Result->Data);
        /// \todo LOCALIZE ME
        ui->webView->setHtml("Unable to retrieve diff because api returned no data for it, debug information:<br><hr>" +
                                HuggleWeb::Encode(this->qd->Result->Data));
        this->diff->stop();
        return;
    }
    // get last id
    if (l.count() > 0)
    {
        QDomElement e = l.at(0).toElement();
        if (e.nodeName() == "rev")
        {
            if (e.attributes().contains("comment"))
            {
                Summary = e.attribute("comment");
            }
        }
    }

    if (Summary == "")
    {
        /// \todo LOCALIZE ME
        Summary = "<font color=red>No summary was provided</font>";
    } else
    {
        Summary = HuggleWeb::Encode(Summary);
    }

    ui->webView->setHtml(Core::HtmlHeader + "<tr></td colspan=2><b>Summary:</b> "
                         + Summary + "</td></tr>" + Diff + Core::HtmlFooter);
    this->diff->stop();
}

void ReportUser::Test()
{
    if (this->tq == NULL)
    {
        this->t2->stop();
        return;
    }

    if (!tq->Processed())
    {
        return;
    }

    QDomDocument d;
    d.setContent(tq->Result->Data);
    QDomNodeList results = d.elementsByTagName("rev");
    ui->pushButton_3->setEnabled(true);
    if (results.count() == 0)
    {
        QMessageBox mb;
        /// \todo LOCALIZE ME
        mb.setText("Error unable to retrieve report page at " + Configuration::LocalConfig_ReportPath);
        mb.exec();
        this->timer->stop();
        delete tq;
        this->tq = NULL;
        return;
    }
    QDomElement e = results.at(0).toElement();
    _p = e.text();
    if (!this->CheckUser())
    {
        QMessageBox mb;
        /// \todo LOCALIZE ME
        mb.setText("This user is already reported");
        mb.exec();
        this->timer->stop();
        delete tq;
        this->user->IsReported = true;
        WikiUser::UpdateUser(user);
        this->tq = NULL;
        return;
    } else
    {
        QMessageBox mb;
        /// \todo LOCALIZE ME
        mb.setText("This user is not reported now");
        mb.exec();
        this->timer->stop();
        delete tq;
        this->tq = NULL;
    }
}

void ReportUser::on_pushButton_clicked()
{
    ui->pushButton->setEnabled(false);
    // we need to get a report info for all selected diffs
    QString reports = "";
    int xx = 0;
    int EvidenceID = 0;
    while (xx < ui->tableWidget->rowCount())
    {
        if (this->CheckBoxes.count() > xx)
        {
            if (this->CheckBoxes.at(xx)->isChecked())
            {
                EvidenceID++;
                reports += "[" + QString(Core::GetProjectScriptURL() + "index.php?title=" +
                                 QUrl::toPercentEncoding(ui->tableWidget->item(xx, 0)->text()) + "&diff="
                                 + ui->tableWidget->item(xx, 3)->text()).toUtf8() + " #" + QString::number(EvidenceID) + "] ";
            }
        }
        xx++;
    }
    if (reports == "")
    {
        QMessageBox::StandardButton mb;
        /// \todo LOCALIZE ME
        mb = QMessageBox::question(this, "Question", "You didn't provide any diffs as evidence, this will make it extremery hard for administrators to figure out if this "\
                   "user is vandal or not. Are you sure you want to continue?", QMessageBox::Yes|QMessageBox::No);
        if (mb == QMessageBox::No)
        {
            ui->pushButton->setEnabled(true);
            return;
        }
    }
    // obtain current page
    Loading = true;
    /// \todo LOCALIZE ME
    ui->pushButton->setText("Retrieving current report page");
    if (this->q != NULL)
    {
        delete q;
    }

    q = new ApiQuery();
    q->SetAction(ActionQuery);
    q->Parameters = "prop=revisions&rvprop=" + QUrl::toPercentEncoding("timestamp|user|comment|content") + "&titles=" +
            QUrl::toPercentEncoding(Configuration::LocalConfig_ReportPath);
    q->Process();
    this->report = reports;
    this->timer->start(800);
    return;
}

void ReportUser::on_pushButton_2_clicked()
{
    QUrl u = QUrl::fromEncoded(QString(Core::GetProjectWikiURL() + QUrl::toPercentEncoding
                                   (this->user->GetTalk()) + "?action=render").toUtf8());
    ui->webView->load(u);
}

void ReportUser::on_tableWidget_clicked(const QModelIndex &index)
{
    /// \todo LOCALIZE ME
    ui->webView->setHtml("Please wait...");
    this->diff->stop();
    if (this->qd != NULL)
    {
        this->qd->Kill();
        this->qd->SafeDelete();
    }
    this->qd = new ApiQuery();
    this->qd->Parameters = "prop=revisions&rvprop=" + QUrl::toPercentEncoding( "ids|user|timestamp|comment" ) + "&rvlimit=1&rvtoken=rollback&rvstartid=" +
            ui->tableWidget->item(index.row(), 3)->text() + "&rvendid=" + ui->tableWidget->item(index.row(), 3)->text() + "&rvdiffto=prev&titles=" +
            QUrl::toPercentEncoding(ui->tableWidget->item(index.row(), 0)->text());
    this->qd->SetAction(ActionQuery);
    this->qd->Process();
    this->diff->start(200);
}

bool ReportUser::CheckUser()
{
    if (_p.contains(this->user->Username))
    {
        return false;
    }
    return true;
}

void ReportUser::InsertUser()
{
    QString xx = Configuration::LocalConfig_IPVTemplateReport;
    if (!this->user->IsIP())
    {
        xx = Configuration::LocalConfig_RUTemplateReport;
    }
    xx = xx.replace("$1", this->user->Username);
    xx = xx.replace("$2", report);
    xx = xx.replace("$3", ui->lineEdit->text());
    _p = _p + "\n" + xx;
}

void ReportUser::on_pushButton_3_clicked()
{
    if (this->tq != NULL)
    {
        delete this->tq;
    }

    this->tq = new ApiQuery();
    this->tq->SetAction(ActionQuery);
    this->tq->Parameters = "prop=revisions&rvprop=" + QUrl::toPercentEncoding("timestamp|user|comment|content") + "&titles=" +
            QUrl::toPercentEncoding(Configuration::LocalConfig_ReportPath);
    this->tq->Process();
    if (this->t2 == NULL)
    {
        this->t2 = new QTimer(this);
    }
    connect(this->t2, SIGNAL(timeout()), this, SLOT(Test()));
    this->t2->start(100);
}
