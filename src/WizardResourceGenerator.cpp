/* Copyright(C) 2016 Björn Stresing, Denis Manthey, Wolfgang Ruppel, Krispin Weiss
 *
 * This program is free software : you can redistribute it and / or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.If not, see <http://www.gnu.org/licenses/>.
 */
#include "WizardResourceGenerator.h"
#include "global.h"
#include "ImfCommon.h"
#include "QtWaitingSpinner.h"
#include "MetadataExtractor.h"
#include "DelegateComboBox.h"
#include <QFileDialog>
#include <QLabel>
#include <QStringList>
#include <QStringListModel>
#include <QStackedLayout>
#include <QTableView>
#include <QPushButton>
#include <QGridLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QImage>
#include <QTimer>
#include <QHeaderView>
#include <QCursor>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <qevent.h>
#include "SMPTE_Labels.h"


WizardResourceGenerator::WizardResourceGenerator(QWidget *pParent /*= NULL*/, QVector<EditRate> rEditRates /* = QVector<EditRates>()*/, QSharedPointer<AssetMxfTrack> rAsset /* = 0 */) :
QWizard(pParent) {

	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	setWindowModality(Qt::WindowModal);
	setWizardStyle(QWizard::ModernStyle);
	setStyleSheet("QWizard QPushButton {min-width: 60 px;}");
	mEditRates = rEditRates;
	mAsset = rAsset;
	mReadOnly = (mAsset != NULL);
	if (mReadOnly)
		setWindowTitle(tr("Resource Generator"));
	else
		setWindowTitle(tr("Metadata View"));
	InitLayout();
}

QSize WizardResourceGenerator::sizeHint() const {

	return QSize(600, 600);
}

void WizardResourceGenerator::InitLayout() {

	WizardResourceGeneratorPage *p_wizard_page = new WizardResourceGeneratorPage(this, mEditRates, mReadOnly, mAsset);
	mPageId = addPage(p_wizard_page);
	setOption(QWizard::HaveCustomButton1, true);
	setButtonText(QWizard::CustomButton1, tr("Browse"));
	QList<QWizard::WizardButton> layout;
	if (mReadOnly)
		layout << QWizard::Stretch << QWizard::CancelButton;
	else
		layout << QWizard::CustomButton1 << QWizard::Stretch << QWizard::CancelButton << QWizard::FinishButton;
	setButtonLayout(layout);

	connect(button(QWizard::CustomButton1), SIGNAL(clicked()), p_wizard_page, SLOT(ShowFileDialog()));
	if (mReadOnly && (mAsset != NULL)) { //Display metadata of existing asset
		switch(mAsset->GetEssenceType()) {
		case Metadata::Jpeg2000:
		case Metadata::ProRes:
#ifdef CODEC_HTJ2K
		case Metadata::HTJ2K:
#endif
			SwitchMode(eMode::Jpeg2000Mode);
			break;
#ifdef APP5_ACES
		case Metadata::Aces:
			SwitchMode(eMode::ExrMode);
			break;
#endif
		case Metadata::Pcm:
			SwitchMode(eMode::WavMode);
			break;
		case Metadata::TimedText:
			SwitchMode(eMode::TTMLMode);
			break;
		case Metadata::ISXD:
			SwitchMode(eMode::ISXDMode);
			break;
		case Metadata::SADM:
			SwitchMode(eMode::MGAMode);
			break;
		case Metadata::ADM:
			SwitchMode(eMode::ADMMode);
			break;
		default:
			break;
		}
	}
}

void WizardResourceGenerator::SwitchMode(eMode mode) {

	WizardResourceGeneratorPage *p_wizard_page = qobject_cast<WizardResourceGeneratorPage*>(page(mPageId));
	if(p_wizard_page) {
		p_wizard_page->SwitchMode(mode);
	}
}

WizardResourceGeneratorPage::WizardResourceGeneratorPage(QWidget *pParent /*= NULL*/, QVector<EditRate> rEditRates /* = QVector<EditRate>()*/, bool rReadOnly, QSharedPointer<AssetMxfTrack> rAsset ) :
QWizardPage(pParent), mpFileDialog(NULL), mpSoundFieldGroupModel(NULL), mpTimedTextModel(NULL), mpTableViewExr(NULL), mpTableViewWav(NULL), mpTableViewTimedText(NULL), mpProxyImageWidget(NULL), mpStackedLayout(NULL), mpComboBoxEditRate(NULL),
mpComboBoxSoundfieldGroup(NULL), mpMsgBox(NULL), mpAs02Wrapper(NULL), mpLineEditDuration(NULL), mpComboBoxCplEditRateTT(NULL),mpComboBoxCplEditRateISXD(NULL), mpComboBoxNamespaceURI(NULL) {
	mpAs02Wrapper = new MetadataExtractor(this);
	mReadOnly = rReadOnly;
	mAsset = rAsset;
	if (!mReadOnly) setTitle(tr("Edit Resource"));
	else setTitle(tr("Metadata view"));
	if (!mReadOnly) setSubTitle(tr("Select essence file(s) that should be wrapped as MXF AS-02."));
	else setSubTitle(tr("MXF metadata items cannot be edited!"));
	mEditRates = rEditRates;
	InitLayout();
}

void WizardResourceGeneratorPage::InitLayout() {

	mpMsgBox = new QMessageBox(this);
	mpMsgBox->setMinimumSize(400, 300);
	mpMsgBox->setIcon(QMessageBox::Warning);
	if (mReadOnly) this->setStyleSheet("QLineEdit, QComboBox {color: #b1b1b1;}");

	mpFileDialog = new QFileDialog(this, QString(), QDir::homePath());
	mpFileDialog->setFileMode(QFileDialog::ExistingFiles);
	mpFileDialog->setViewMode(QFileDialog::Detail);
	mpFileDialog->setNameFilters(QStringList() << "*.exr" << "*.wav" << "*.ttml");
	mpFileDialog->setIconProvider(new IconProviderExrWav(this)); // TODO: Does not work.

	mpProxyImageWidget = new WidgetProxyImage(this);

	mpComboBoxEditRate = new QComboBox(this);
	mpComboBoxEditRate->setWhatsThis(tr("The edit rate can be changed later as long as the resource is not played out."));
	QStringListModel *p_edit_rates_model = new QStringListModel(this);
	p_edit_rates_model->setStringList(EditRate::GetFrameRateNames());
	mpComboBoxEditRate->setModel(p_edit_rates_model);

	mpComboBoxNamespaceURI = new QComboBox(this);
	mpComboBoxNamespaceURI->setWhatsThis(tr("An IMSC1.1 profile to which the Document Instance conforms."));
	QStringListModel *p_namespace_model = new QStringListModel(this);
	QStringList imsc_namespaces;
	imsc_namespaces << IMSC_11_PROFILE_TEXT << IMSC_11_PROFILE_IMAGE << IMSC_10_PROFILE_TEXT << IMSC_10_PROFILE_IMAGE << NETFLIX_IMSC11_TT << EBU_TT_D << TTML10_DP_US;
	p_namespace_model->setStringList(imsc_namespaces);
	mpComboBoxNamespaceURI->setModel(p_namespace_model);


	//--- soundfield group ---
	mpComboBoxSoundfieldGroup = new QComboBox(this);
	mpComboBoxSoundfieldGroup->setWhatsThis(tr("Select a soundfield group. Every soundfield group channel must be assigned."));
	QStringListModel *p_sound_field_group_model = new QStringListModel(this);
	p_sound_field_group_model->setStringList(SoundfieldGroup::GetSoundFieldGroupNames());
	mpComboBoxSoundfieldGroup->setModel(p_sound_field_group_model);
	if (mReadOnly) mpComboBoxSoundfieldGroup->setDisabled(true);
	mpSoundFieldGroupModel = new SoundFieldGroupModel(this);
	mpTableViewWav = new QTableView(this);
	mpTableViewWav->setFocusPolicy(Qt::NoFocus);
	mpTableViewWav->setModel(mpSoundFieldGroupModel);
	mpTableViewWav->setEditTriggers(QAbstractItemView::AllEditTriggers);
	mpTableViewWav->setSelectionBehavior(QAbstractItemView::SelectRows);
	mpTableViewWav->setSelectionMode(QAbstractItemView::NoSelection);
	mpTableViewWav->setShowGrid(false);
	mpTableViewWav->horizontalHeader()->setHidden(true);
	mpTableViewWav->horizontalHeader()->setStretchLastSection(true);
	mpTableViewWav->verticalHeader()->setHidden(true);
	mpTableViewWav->resizeRowsToContents();
	mpTableViewWav->resizeColumnsToContents();
	mpTableViewWav->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	mpTableViewWav->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	mpTableViewWav->setItemDelegateForColumn(SoundFieldGroupModel::ColumnDstChannel, new DelegateComboBox(this, false, false));
	//WR
	//language code is two or three lowercase letters, region code is either two uppercase letters or three digits"
	const QRegularExpression rx_lang("[a-z]{2,3}\\-([A-Z]{2}|[0-9]{3})");
	QRegularExpressionValidator *v_lang = new QRegularExpressionValidator(rx_lang, this);
	mpLineEditLanguageTagWav = new QLineEdit(this);
	mpLineEditLanguageTagWav->setAlignment(Qt::AlignRight);
	mpLineEditLanguageTagWav->setText("en-US");
	mpLineEditLanguageTagWav->setValidator(v_lang);
	if (mReadOnly) mpLineEditLanguageTagWav->setDisabled(true);
	connect(mpLineEditLanguageTagWav, SIGNAL(textEdited(QString)), this, SLOT(languageTagWavChanged()));
	QRegularExpressionValidator *v_lang2 = new QRegularExpressionValidator(rx_lang, this);
	mpLineEditLanguageTagTT = new QLineEdit(this);
	mpLineEditLanguageTagTT->setAlignment(Qt::AlignRight);
	mpLineEditLanguageTagTT->setText("en-US");
	mpLineEditLanguageTagTT->setValidator(v_lang2);
	if (mReadOnly) mpLineEditLanguageTagTT->setDisabled(true);
	connect(mpLineEditLanguageTagTT, SIGNAL(textEdited(QString)), this, SLOT(languageTagTTChanged()));
	// Allows for whitespace and slash, but not as first character
	static QRegularExpression mca_items("[0-9a-zA@]{1}[0-9a-zA-Z_\\s/]*");
	QRegularExpressionValidator *v_mca_items = new QRegularExpressionValidator(mca_items, this);
	mpLineEditMCATitle = new QLineEdit(this);
	mpLineEditMCATitle->setAlignment(Qt::AlignRight);
	mpLineEditMCATitle->setText("n/a");
	mpLineEditMCATitle->setValidator(v_mca_items);
	if (mReadOnly) mpLineEditMCATitle->setDisabled(true);
	//mpLineEditMCATitle->selectAll();
	//connect(mpLineEditMCATitle, SIGNAL(clicked()), mpLineEditMCATitle, SLOT(selectAll()));
	//connect(mpLineEditMCATitle, SIGNAL(textEdited(QString)), this, SLOT(mcaTitleChanged()));
	mpLineEditMCATitleVersion = new QLineEdit(this);
	mpLineEditMCATitleVersion->setAlignment(Qt::AlignRight);
	mpLineEditMCATitleVersion->setText("n/a");
	mpLineEditMCATitleVersion->setValidator(v_mca_items);
	if (mReadOnly) mpLineEditMCATitleVersion->setDisabled(true);
	//connect(mpLineEditMCATitleVersion, SIGNAL(textEdited(QString)), this, SLOT(mcaTitleVersionChanged()));
	mpLineEditMCAAudioContentKind = new QComboBox(this);
	mpLineEditMCAAudioContentKind->setEditable(true);
	mpLineEditMCAAudioContentKind->lineEdit()->setAlignment(Qt::AlignRight);
	if (!mReadOnly) mpLineEditMCAAudioContentKind->lineEdit()->setPlaceholderText("Pick a symbol from the list");
	mpLineEditMCAAudioContentKind->addItems(mMCAContentSymbols);
	mpLineEditMCAAudioContentKind->setCurrentIndex(-1);
	mpLineEditMCAAudioContentKind->setValidator(v_mca_items);
	if (mReadOnly) mpLineEditMCAAudioContentKind->setDisabled(true);
	mpLineEditMCAContent = new QComboBox(this);
	mpLineEditMCAContent->setEditable(true);
	mpLineEditMCAContent->lineEdit()->setAlignment(Qt::AlignRight);
	if (!mReadOnly) mpLineEditMCAContent->lineEdit()->setPlaceholderText("Pick a symbol from the list");
	mpLineEditMCAContent->addItems(mMCAContentSymbols);
	mpLineEditMCAContent->setCurrentIndex(-1);
	mpLineEditMCAContent->setValidator(v_mca_items);
	if (mReadOnly) mpLineEditMCAContent->setDisabled(true);
	//connect(mpLineEditMCAContent, SIGNAL(textEdited(QString)), this, SLOT(mcaAudioContentKindChanged()));
	mpLineEditMCAAudioElementKind = new QComboBox(this);
	mpLineEditMCAAudioElementKind->setEditable(true);
	mpLineEditMCAAudioElementKind->lineEdit()->setAlignment(Qt::AlignRight);
	if (!mReadOnly) mpLineEditMCAAudioElementKind->lineEdit()->setPlaceholderText("Pick a symbol from the list");
	mpLineEditMCAAudioElementKind->addItems(mMCAUseClassSymbols);
	mpLineEditMCAAudioElementKind->setCurrentIndex(-1);
	mpLineEditMCAAudioElementKind->setValidator(v_mca_items);
	if (mReadOnly) mpLineEditMCAAudioElementKind->setDisabled(true);
	//connect(mpLineEditMCAAudioElementKind, SIGNAL(textEdited(QString)), this, SLOT(mcaAudioElementKindChanged()));
	mpComboBoxCplEditRateTT = new QComboBox(this);
	mpComboBoxCplEditRateTT->setWhatsThis(tr("Select a frame rate. It shall match the CPL Edit Rate"));
	mpComboBoxCplEditRateISXD = new QComboBox(this);
	mpComboBoxCplEditRateISXD->setWhatsThis(tr("Select a frame rate. It shall match the CPL Edit Rate"));
	QStringListModel *p_edit_rate_group_model = new QStringListModel(this);
	QStringList p_edit_rate_string_list;
	if (mReadOnly) {
		p_edit_rate_string_list << mAsset->GetEditRate().GetName();
	} else {
		if (!mEditRates.isEmpty()) {
			for (QVector<EditRate>::iterator i=mEditRates.begin(); i < mEditRates.end(); i++) {
				p_edit_rate_string_list << i->GetName();
			}
		} else { //NO CPL in IMP yet
			p_edit_rate_string_list << "Select an Edit Rate";
			p_edit_rate_string_list << EditRate::GetFrameRateNames();
		}
	}
	p_edit_rate_group_model->setStringList(p_edit_rate_string_list);
	mpComboBoxCplEditRateTT->setModel(p_edit_rate_group_model);
	if (mReadOnly) mpComboBoxCplEditRateTT->setDisabled(true);
	mpComboBoxCplEditRateISXD->setModel(p_edit_rate_group_model);
	if (mReadOnly) mpComboBoxCplEditRateISXD->setDisabled(true);
	mpLineEditNamespaceURITT = new QLineEdit(this);
	mpLineEditNamespaceURITT->setDisabled(true);
	mpLineEditNamespaceURITT->setStyleSheet("QLineEdit {color: #b1b1b1;}");
	mpLineEditNamespaceURIISXD = new QLineEdit(this);
	mpLineEditNamespaceURIISXD->setDisabled(true);
	mpLineEditNamespaceURIISXD->setStyleSheet("QLineEdit {color: #b1b1b1;}");
	mpLineEditDurationReadOnlyTT = new QLineEdit(this);
	mpLineEditDurationReadOnlyTT->setDisabled(true);
	mpLineEditDurationReadOnlyTT->setStyleSheet("QLineEdit {color: #b1b1b1;}");
	mpLineEditDurationReadOnlyISXD = new QLineEdit(this);
	mpLineEditDurationReadOnlyISXD->setDisabled(true);
	mpLineEditDurationReadOnlyISXD->setStyleSheet("QLineEdit {color: #b1b1b1;}");

	//connect(mpLineEditNamespaceURI, SIGNAL(textEdited(QString)), this, SLOT(languageTagTTChanged()));

	//WR

			/* -----Denis Manthey----- */

	mpTimedTextModel = new TimedTextModel(this);
	mpTableViewTimedText = new QTableView(this);
	mpTableViewTimedText->setFocusPolicy(Qt::NoFocus);
	mpTableViewTimedText->setModel(mpTimedTextModel);
	mpTableViewTimedText->setEditTriggers(QAbstractItemView::NoEditTriggers);
	mpTableViewTimedText->setSelectionBehavior(QAbstractItemView::SelectRows);
	mpTableViewTimedText->setSelectionMode(QAbstractItemView::SingleSelection);
	mpTableViewTimedText->setShowGrid(false);
	mpTableViewTimedText->horizontalHeader()->setHidden(true);
	mpTableViewTimedText->horizontalHeader()->setStretchLastSection(true);
	mpTableViewTimedText->verticalHeader()->setHidden(true);
	mpTableViewTimedText->resizeRowsToContents();
	mpTableViewTimedText->resizeColumnsToContents();

	QPushButton *pGenNew = new QPushButton("Generate Empty Timed Text Resource");
	pGenNew->setAutoDefault(false);
	mpGroupBox = new QGroupBox;
	mpDirDialog = new QFileDialog(this, QString(), QDir::homePath());
	mpDirDialog->setFileMode(QFileDialog::Directory);
	mpDirDialog->setOption(QFileDialog::ShowDirsOnly);
	mpLineEditFileDir = new QLineEdit(this);
	mpLineEditFileDir->setEnabled(false);
	mpLineEditFileDir->setAlignment(Qt::AlignRight);
	QPushButton *pBrowseDir = new QPushButton(this);
	pBrowseDir->setText(tr("Browse"));
	pBrowseDir->setAutoDefault(false);
	//connect(pBrowseDir, SIGNAL(clicked(bool)), this, SLOT(ShowDirDialog()));
	connect(mpDirDialog, SIGNAL(fileSelected(const QString &)), mpLineEditFileDir, SLOT(setText(const QString &)));
	static QRegularExpression rx("[A-Za-z0-9-_]+");
	QRegularExpressionValidator *v = new QRegularExpressionValidator(rx, this);
	mpLineEditFileName = new QLineEdit(this);
	mpLineEditFileName->setAlignment(Qt::AlignRight);
	mpLineEditFileName->setPlaceholderText("file name");
	mpLineEditFileName->setValidator(v);
	mpLineEditDuration = new QLineEdit(this);
	mpLineEditDuration->setAlignment(Qt::AlignRight);
	mpLineEditDuration->setPlaceholderText("Duration [frames]");
	mpLineEditDurationEmptyTTGenerator = new QLineEdit(this);
	mpLineEditDurationEmptyTTGenerator->setAlignment(Qt::AlignRight);
	mpLineEditDurationEmptyTTGenerator->setPlaceholderText("Duration [frames]");
	mpLineEditDurationEmptyTTGenerator->setValidator(new QIntValidator(this));
	mpGenerateEmpty_button = new QPushButton(this);
	mpGenerateEmpty_button->setText(tr("Generate"));
	mpGenerateEmpty_button->setAutoDefault(false);
	mpGenerateEmpty_button->setEnabled(false);
	connect(pBrowseDir, SIGNAL(clicked(bool)), this, SLOT(ShowDirDialog()));
	connect(mpDirDialog, SIGNAL(fileSelected(const QString &)), mpLineEditFileDir, SLOT(setText(const QString &)));
	connect(pGenNew,SIGNAL(clicked()),this,SLOT(hideGroupBox()));
	//connect(mpLineEditDuration, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
	connect(mpLineEditDurationEmptyTTGenerator, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
	connect(mpLineEditFileName, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
	connect(mpLineEditFileDir, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
	connect(mpGenerateEmpty_button, SIGNAL(clicked(bool)), this, SLOT(GenerateEmptyTimedText()));
	QLabel *GenNew = new QLabel(this);
	GenNew->setStyleSheet("font: bold; text-decoration: underline");
	GenNew->setText("Generate Empty Timed Text Resource");

			/* -----Denis Manthey----- */


	QWidget *p_wrapper_widget_one = new QWidget(this);
	QGridLayout *p_wrapper_layout_one = new QGridLayout();
	p_wrapper_layout_one->setContentsMargins(0, 0, 0, 0);
	p_wrapper_layout_one->addWidget(new QLabel(tr("Frame Rate:"), this), 0, 0, 1, 1);
	p_wrapper_layout_one->addWidget(mpComboBoxEditRate, 0, 1, 1, 1);
	QWidget *p_wrapper_widget_two = new QWidget(this);
	QGridLayout *p_wrapper_layout_two = new QGridLayout();
	p_wrapper_layout_two->setContentsMargins(0, 0, 0, 0);
	p_wrapper_layout_two->addWidget(new QLabel(tr("Soundfield group:"), this), 0, 0, 1, 1);
	p_wrapper_layout_two->addWidget(mpComboBoxSoundfieldGroup, 0, 1, 1, 1);
	p_wrapper_layout_two->addWidget(new QLabel(tr("RFC 5646 Language Tag (e.g. en-US):"), this), 1, 0, 1, 1);
	p_wrapper_layout_two->addWidget(mpLineEditLanguageTagWav, 1, 1, 1, 1);
	QLabel* mca_title = new QLabel(tr("MCA Title:"), this);
	mca_title->setToolTip("MCA Title shall be present. See https://www.imfug.com/TR/audio-track-files/ for more information.");
	p_wrapper_layout_two->addWidget(mca_title, 2, 0, 1, 1);
	p_wrapper_layout_two->addWidget(mpLineEditMCATitle, 2, 1, 1, 1);
	QLabel* mca_title_version = new QLabel(tr("MCA Title Version:"), this);
	mca_title_version->setToolTip("MCA Title Version shall be present. See https://www.imfug.com/TR/audio-track-files/ for more information.");
	p_wrapper_layout_two->addWidget(mca_title_version, 3, 0, 1, 1);
	p_wrapper_layout_two->addWidget(mpLineEditMCATitleVersion, 3, 1, 1, 1);
	QLabel* mca_content_kind = new QLabel(tr("MCA Audio Content Kind:"), this);
	mca_content_kind->setToolTip("MCA Audio Content Kind shall be present. See https://www.imfug.com/TR/audio-track-files/ for more information.");
	p_wrapper_layout_two->addWidget(mca_content_kind, 4, 0, 1, 1);
	p_wrapper_layout_two->addWidget(mpLineEditMCAAudioContentKind, 4, 1, 1, 1);
	QLabel* mca_element_kind = new QLabel(tr("MCA Audio Element Kind:"), this);
	mca_element_kind->setToolTip("MCA Audio Element Kind shall be present. See https://www.imfug.com/TR/audio-track-files/ for more information.");
	p_wrapper_layout_two->addWidget(mca_element_kind, 5, 0, 1, 1);
	p_wrapper_layout_two->addWidget(mpLineEditMCAAudioElementKind, 5, 1, 1, 1);
	p_wrapper_layout_two->addWidget(mpTableViewWav, 6, 0, 1, 2);
	p_wrapper_widget_two->setLayout(p_wrapper_layout_two);


			/* -----Denis Manthey----- */

	QWidget *p_wrapper_widget_three = new QWidget(this);
	QGridLayout *p_wrapper_layout_three = new QGridLayout();
	QGridLayout *vbox = new QGridLayout;
	p_wrapper_layout_three->setContentsMargins(0, 0, 0, 0);
	if (!mReadOnly) p_wrapper_layout_three->addWidget(new QLabel(tr("Select a Timed Text Resource (.xml) compliant to IMSC1.1"), this), 0, 0, 1, 3);
	p_wrapper_layout_three->addWidget(new QLabel(tr("Edit Rate:"), this), 1, 0, 1, 2);
	p_wrapper_layout_three->addWidget(mpComboBoxCplEditRateTT, 1, 2, 1, 1);
	p_wrapper_layout_three->addWidget(new QLabel(tr("RFC 5646 Language Tag (e.g. en-US):"), this), 2, 0, 1, 2);
	p_wrapper_layout_three->addWidget(mpLineEditLanguageTagTT, 2, 2, 1, 1);
	p_wrapper_layout_three->addWidget(new QLabel(tr("Duration (frames):"), this), 3, 0, 1, 2);
	p_wrapper_layout_three->addWidget(mpLineEditDurationReadOnlyTT, 3, 2, 1, 1);
	p_wrapper_layout_three->addWidget(new QLabel(tr("NamespaceURI:"), this), 4, 0, 1, 2);
	p_wrapper_layout_three->addWidget(mpLineEditNamespaceURITT, 4, 2, 1, 1);
	p_wrapper_layout_three->addWidget(mpTableViewTimedText, 5, 0, 1, 3);
	if (!mReadOnly) p_wrapper_layout_three->addWidget(pGenNew, 6, 0, 1, 3);

/*
	vbox->addWidget(new QLabel(tr("Set the file name of the empty tt resource:"), this), 1, 0, 1, 1);
	vbox->addWidget(mpLineEditFileName, 1, 1, 1, 1);
	vbox->addWidget(new QLabel(tr(".xml"), this), 1, 2, 1, 1);
	vbox->addWidget(new QLabel(tr("Set the directory of the empty tt resource:"), this), 2, 0, 1, 1);
	vbox->addWidget(mpLineEditFileDir, 2, 1, 1, 1);
	vbox->addWidget(pBrowseDir, 2, 2, 1, 1);
*/
	vbox->addWidget(new QLabel(tr("Set the duration of the empty tt resource:"), this), 1, 0, 1, 1);
	vbox->addWidget(mpLineEditDurationEmptyTTGenerator, 1, 1, 1, 1);
	vbox->addWidget(mpGenerateEmpty_button, 2, 1, 1, 1);
	mpGroupBox->setLayout(vbox);
	mpGroupBox->hide();

	p_wrapper_layout_three->addWidget(mpGroupBox, 5, 0, 1, 3);
	p_wrapper_widget_three->setLayout(p_wrapper_layout_three);


	mpEditDurationDialog = new QDialog(this);
	QGridLayout *mpEditDurationDialogLay = new QGridLayout();
	QPushButton *pOk = new QPushButton("OK");
	QPushButton *pCancel = new QPushButton("Cancel");

	mpEditDurationDialogLay->addWidget(new QLabel(tr("Duration could not be resolved.\n\nEnter a higher duration than needed.\nDuration can be shortened in the timeline"), this), 0, 0, 1, 2);
	mpEditDurationDialogLay->addWidget(mpLineEditDuration, 1, 0, 1, 2);
	mpEditDurationDialogLay->addWidget(pOk, 2, 0, 1, 1);
	mpEditDurationDialogLay->addWidget(pCancel, 2, 1, 1, 1);

	mpEditDurationDialog->setLayout(mpEditDurationDialogLay);

	connect(pCancel, SIGNAL(clicked(bool)), mpEditDurationDialog, SLOT(close()));
	connect(pOk, SIGNAL(clicked(bool)), mpEditDurationDialog, SLOT(accept()));

	mpSelectNamespaceDialog = new QDialog(this);
	QGridLayout *mpSelectNamespaceDialogLayout = new QGridLayout();
	QPushButton *pNamsepaceOk = new QPushButton("OK");
	QPushButton *mpSelectNamespaceDialogCancel = new QPushButton("Cancel");

	mpSelectNamespaceDialogLayout->addWidget(new QLabel(tr("No IMSC1.1 profile found in TTML file.\n\nSelect an appropriate profile from the list"), this), 0, 0, 1, 2);
	mpSelectNamespaceDialogLayout->addWidget(mpComboBoxNamespaceURI, 1, 0, 1, 2);
	mpSelectNamespaceDialogLayout->addWidget(pNamsepaceOk, 2, 0, 1, 1);
	mpSelectNamespaceDialogLayout->addWidget(mpSelectNamespaceDialogCancel, 2, 1, 1, 1);

	mpSelectNamespaceDialog->setLayout(mpSelectNamespaceDialogLayout);

	connect(mpSelectNamespaceDialogCancel, SIGNAL(clicked(bool)), mpSelectNamespaceDialog, SLOT(close()));
	connect(pNamsepaceOk, SIGNAL(clicked(bool)), mpSelectNamespaceDialog, SLOT(accept()));


			/* -----Denis Manthey----- */

	QWidget *p_wrapper_widget_four = new QWidget(this);
	QGridLayout *p_wrapper_layout_four = new QGridLayout();
	p_wrapper_layout_four->setContentsMargins(0, 0, 0, 0);

	if (mReadOnly && mAsset) {
		int i = -1;
		Metadata metadata = mAsset->GetMetadata();
		QLabel* label = new QLabel();
		label->setTextInteractionFlags(Qt::TextSelectableByMouse);
		label->setText(metadata.assetId.toString());
		p_wrapper_layout_four->addWidget(new QLabel(tr("Track File ID:")), ++i, 0, 1, 1);
		p_wrapper_layout_four->addWidget(label, i, 1, 1, 1);
		label = new QLabel();
		label->setTextInteractionFlags(Qt::TextSelectableByMouse);
		label->setText(metadata.pictureEssenceCoding);
		p_wrapper_layout_four->addWidget(new QLabel(tr("Picture Essence Encoding UL:")), ++i, 0, 1, 1);
		p_wrapper_layout_four->addWidget(label, i, 1, 1, 1);
		p_wrapper_layout_four->addWidget(new QLabel(tr("Picture Essence Encoding:")), ++i, 0, 1, 1);
		if (SMPTE::J2K_ProfilesMap.contains(metadata.pictureEssenceCoding))
			p_wrapper_layout_four->addWidget(new QLabel(SMPTE::vJ2K_Profiles[SMPTE::J2K_ProfilesMap[metadata.pictureEssenceCoding]]), i, 1, 1, 1);
		else if (SMPTE::ACES_ProfilesMap.contains(metadata.pictureEssenceCoding))
			p_wrapper_layout_four->addWidget(new QLabel(SMPTE::vACES_Profiles[SMPTE::ACES_ProfilesMap[metadata.pictureEssenceCoding]]), i, 1, 1, 1);
		else if (SMPTE::ProRes_ProfilesMap.contains(metadata.pictureEssenceCoding))
			p_wrapper_layout_four->addWidget(new QLabel(SMPTE::vProRes_Profiles[SMPTE::ProRes_ProfilesMap[metadata.pictureEssenceCoding]]), i, 1, 1, 1);
		else
			p_wrapper_layout_four->addWidget(new QLabel("Unknown"), ++i, 1, 1, 1);
		p_wrapper_layout_four->addWidget(new QLabel(tr("Duration:")), ++i, 0, 1, 1);
		if(metadata.duration.IsValid() && metadata.editRate.IsValid())
			p_wrapper_layout_four->addWidget(new QLabel(QString::number(metadata.duration.GetCount())+ " frames"), i, 1, 1, 1);
		p_wrapper_layout_four->addWidget(new QLabel(tr("Frame Rate:")), ++i, 0, 1, 1);
		if(metadata.editRate.IsValid() == true)
			p_wrapper_layout_four->addWidget(new QLabel(metadata.editRate.GetName()), i, 1, 1, 1);
		p_wrapper_layout_four->addWidget(new QLabel(tr("Stored Resolution:")), ++i, 0, 1, 1);
		if(metadata.storedHeight != 0 || metadata.storedWidth != 0)
			p_wrapper_layout_four->addWidget(new QLabel(QObject::tr("%1 x %2").arg(metadata.storedWidth).arg(metadata.storedHeight)), i, 1, 1, 1);
		p_wrapper_layout_four->addWidget(new QLabel(tr("Displayed Resolution:")), ++i, 0, 1, 1);
		if(metadata.displayHeight != 0 || metadata.displayWidth != 0)
			p_wrapper_layout_four->addWidget(new QLabel(QObject::tr("%1 x %2").arg(metadata.displayWidth).arg(metadata.displayHeight)), i, 1, 1, 1);
		p_wrapper_layout_four->addWidget(new QLabel(tr("Aspect Ratio:")), ++i, 0, 1, 1);
		if(metadata.aspectRatio != ASDCP::Rational())
			p_wrapper_layout_four->addWidget(new QLabel(QObject::tr("%1 (%2:%3)").arg(metadata.aspectRatio.Quotient()).arg(metadata.aspectRatio.Numerator).arg(metadata.aspectRatio.Denominator)), i, 1, 1, 1);
		p_wrapper_layout_four->addWidget(new QLabel(tr("Color Mode:")), ++i, 0, 1, 1);
		if(metadata.horizontalSubsampling != 0 && metadata.colorEncoding != metadata.Unknown_Color_Encoding) {
			if(metadata.colorEncoding == Metadata::RGBA)
				p_wrapper_layout_four->addWidget(new QLabel("RGB"), i, 1, 1, 1);
			else if(metadata.colorEncoding == Metadata::CDCI)
				p_wrapper_layout_four->addWidget(new QLabel("YCbCr"), i, 1, 1, 1);
		}
		p_wrapper_layout_four->addWidget(new QLabel(tr("Color Sampling:")), ++i, 0, 1, 1);
		if(metadata.horizontalSubsampling != 0)
			p_wrapper_layout_four->addWidget(new QLabel(QObject::tr("%1:%2:%3").arg(4).arg(4 / metadata.horizontalSubsampling).arg(4 / metadata.horizontalSubsampling)), i, 1, 1, 1);
		p_wrapper_layout_four->addWidget(new QLabel(tr("Color Depth:")), ++i, 0, 1, 1);
		if(metadata.componentDepth == 253) {
			p_wrapper_layout_four->addWidget(new QLabel(QObject::tr("16 bit float")), i, 1, 1, 1);
		} else if(metadata.componentDepth != 0) {
			p_wrapper_layout_four->addWidget(new QLabel(QObject::tr("%1 bit").arg(metadata.componentDepth)), i, 1, 1, 1);
		}
		p_wrapper_layout_four->addWidget(new QLabel(tr("Primaries:")), ++i, 0, 1, 1);
		p_wrapper_layout_four->addWidget(new QLabel(SMPTE::vColorPrimaries[metadata.colorPrimaries]), i, 1, 1, 1);
		p_wrapper_layout_four->addWidget(new QLabel(tr("OETF:")), ++i, 0, 1, 1);
		p_wrapper_layout_four->addWidget(new QLabel(SMPTE::vTransferCharacteristic[metadata.transferCharcteristics]), i, 1, 1, 1);
		if (metadata.isPHDR) {
			p_wrapper_layout_four->addWidget(new QLabel(tr("Metadata:")), ++i, 0, 1, 1);
			p_wrapper_layout_four->addWidget(new QLabel(tr("PHDR data present")), i, 1, 1, 1);
		}

		QWidget* empty = new QWidget();
		empty->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
		p_wrapper_layout_four->addWidget(empty);

	}

	p_wrapper_widget_four->setLayout(p_wrapper_layout_four);

	QWidget *p_wrapper_widget_five = new QWidget(this);
	QGridLayout *p_wrapper_layout_five = new QGridLayout();
	p_wrapper_layout_five->setContentsMargins(0, 0, 0, 0);

	int i = -1;
	if (mReadOnly && mAsset) {
		Metadata metadata = mAsset->GetMetadata();
		QLabel* label = new QLabel();
		label->setTextInteractionFlags(Qt::TextSelectableByMouse);
		label->setText(metadata.assetId.toString());
		p_wrapper_layout_five->addWidget(new QLabel(tr("Track File ID:")), ++i, 0, 1, 1);
		p_wrapper_layout_five->addWidget(label, i, 1, 1, 1);
		mpComboBoxCplEditRateISXD->setCurrentText(metadata.editRate.GetName());
		mpComboBoxCplEditRateISXD->setDisabled(true);
	} else {
		p_wrapper_layout_five->addWidget(new QLabel(tr("Select a Folder with ISXD XML (.xml) documents"), this), ++i, 0, 1, 3);
	}
	p_wrapper_layout_five->addWidget(new QLabel(tr("Edit Rate:"), this), ++i, 0, 1, 2);
	p_wrapper_layout_five->addWidget(mpComboBoxCplEditRateISXD, i, 2, 1, 1);
	p_wrapper_layout_five->addWidget(new QLabel(tr("Duration (frames):"), this), ++i, 0, 1, 2);
	p_wrapper_layout_five->addWidget(mpLineEditDurationReadOnlyISXD, i, 2, 1, 1);
	p_wrapper_layout_five->addWidget(new QLabel(tr("NamespaceURI:"), this), ++i, 0, 1, 2);
	p_wrapper_layout_five->addWidget(mpLineEditNamespaceURIISXD, i, 2, 1, 1);
	//p_wrapper_layout_five->addWidget(mpTableViewTimedText, 5, 0, 1, 3);

	QWidget* empty = new QWidget();
	empty->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
	p_wrapper_layout_five->addWidget(empty);

	p_wrapper_widget_five->setLayout(p_wrapper_layout_five);


	// MGA S-ADM
	QWidget *p_wrapper_widget_six = new QWidget(this);
	QGridLayout *p_wrapper_layout_six = new QGridLayout();
	p_wrapper_layout_six->setContentsMargins(0, 0, 0, 0);

	if (mReadOnly && mAsset) {
		int i = -1;
		Metadata metadata = mAsset->GetMetadata();
		p_wrapper_layout_six->addWidget(new QLabel(tr("Duration:")), ++i, 0, 1, 1);
		if(metadata.duration.IsValid() && metadata.editRate.IsValid())
			p_wrapper_layout_six->addWidget(new QLabel(QString::number(metadata.duration.GetCount())+ " frames"), i, 1, 1, 1);
		p_wrapper_layout_six->addWidget(new QLabel(tr("Frame Rate:")), ++i, 0, 1, 1);
		if(metadata.editRate.IsValid() == true)
			p_wrapper_layout_six->addWidget(new QLabel(metadata.editRate.GetName()), i, 1, 1, 1);
		p_wrapper_layout_six->addWidget(new QLabel(tr("Average Bytes per Second:")), ++i, 0, 1, 1);
		p_wrapper_layout_six->addWidget(new QLabel(QString::number(metadata.averageBytesPerSecond)), i, 1, 1, 1);
		Q_FOREACH (const Metadata::MGASoundfieldGroup& mgaSoundFieldGroup, metadata.mgaSoundFieldGroupList) {
			//p_wrapper_layout_six->setVerticalSpacing(50);
			QFrame* line = new QFrame;
			line->setFrameShape(QFrame::HLine);
			p_wrapper_layout_six->addWidget(line, ++i, 0, 1, 2);
			p_wrapper_layout_six->addWidget(new QLabel(tr("Soundfield:")), ++i, 0, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(mgaSoundFieldGroup.soundfieldGroup.GetName()), i, 1, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(tr("Metadata Section Link ID:")), ++i, 0, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(mgaSoundFieldGroup.mgaMetadataSectionLinkId), i, 1, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(tr("ADM Audio Programme ID:")), ++i, 0, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(mgaSoundFieldGroup.admAudioProgrammeID), i, 1, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(tr("MCA Tag Symbol:")), ++i, 0, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(mgaSoundFieldGroup.mcaTagSymbol), i, 1, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(tr("MCA Tag Name:")), ++i, 0, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(mgaSoundFieldGroup.mcaTagName), i, 1, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(tr("Language:")), ++i, 0, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(mgaSoundFieldGroup.mcaSpokenLanguage), i, 1, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(tr("MCA Content:")), ++i, 0, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(mgaSoundFieldGroup.mcaContent), i, 1, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(tr("MCA Use Class:")), ++i, 0, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(mgaSoundFieldGroup.mcaUseClass), i, 1, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(tr("MCA Title:")), ++i, 0, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(mgaSoundFieldGroup.mcaTitle), i, 1, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(tr("MCA Title Version:")), ++i, 0, 1, 1);
			p_wrapper_layout_six->addWidget(new QLabel(mgaSoundFieldGroup.mcaTitleVersion), i, 1, 1, 1);
		}

		QWidget* empty = new QWidget();
		empty->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
		p_wrapper_layout_six->addWidget(empty);

	}

	QScrollArea *p_scroll_area_SADM = new QScrollArea;
	p_scroll_area_SADM->setWidgetResizable(true);
	p_wrapper_widget_six->setLayout(p_wrapper_layout_six);
	p_scroll_area_SADM->setWidget(p_wrapper_widget_six);

	// ADM Audio
	QWidget *p_wrapper_widget_seven = new QWidget(this);
	QGridLayout *p_wrapper_layout_seven = new QGridLayout();
	p_wrapper_layout_seven->setContentsMargins(0, 0, 0, 0);

	if (mReadOnly && mAsset) {
		int i = -1;
		Metadata metadata = mAsset->GetMetadata();
		p_wrapper_layout_seven->addWidget(new QLabel(tr("Duration:")), ++i, 0, 1, 1);
		if(metadata.duration.IsValid() && metadata.editRate.IsValid())
			p_wrapper_layout_seven->addWidget(new QLabel(QString::number(metadata.duration.GetCount())+ " frames"), i, 1, 1, 1);
		p_wrapper_layout_seven->addWidget(new QLabel(tr("Frame Rate:")), ++i, 0, 1, 1);
		if(metadata.editRate.IsValid() == true)
			p_wrapper_layout_seven->addWidget(new QLabel(metadata.editRate.GetName()), i, 1, 1, 1);
		p_wrapper_layout_seven->addWidget(new QLabel(tr("Average Bytes per Second:")), ++i, 0, 1, 1);
		p_wrapper_layout_seven->addWidget(new QLabel(QString::number(metadata.averageBytesPerSecond)), i, 1, 1, 1);
		Q_FOREACH (const Metadata::ADMSoundfieldGroup& admSoundFieldGroup, metadata.admSoundFieldGroupList) {
			QFrame* line = new QFrame;
			line->setFrameShape(QFrame::HLine);
			p_wrapper_layout_seven->addWidget(line, ++i, 0, 1, 2);
			p_wrapper_layout_seven->addWidget(new QLabel(tr("Soundfield:")), ++i, 0, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(admSoundFieldGroup.soundfieldGroup.GetName()), i, 1, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(tr("RIFF Chunk Stream ID_link2:")), ++i, 0, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(QString::number(admSoundFieldGroup.admRIFFChunkStreamID_link2)), i, 1, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(tr("ADM Audio Programme ID:")), ++i, 0, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(admSoundFieldGroup.admAudioProgrammeID), i, 1, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(tr("MCA Tag Symbol:")), ++i, 0, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(admSoundFieldGroup.mcaTagSymbol), i, 1, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(tr("MCA Tag Name:")), ++i, 0, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(admSoundFieldGroup.mcaTagName), i, 1, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(tr("Language:")), ++i, 0, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(admSoundFieldGroup.mcaSpokenLanguage), i, 1, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(tr("MCA Content:")), ++i, 0, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(admSoundFieldGroup.mcaContent), i, 1, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(tr("MCA Use Class:")), ++i, 0, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(admSoundFieldGroup.mcaUseClass), i, 1, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(tr("MCA Title:")), ++i, 0, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(admSoundFieldGroup.mcaTitle), i, 1, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(tr("MCA Title Version:")), ++i, 0, 1, 1);
			p_wrapper_layout_seven->addWidget(new QLabel(admSoundFieldGroup.mcaTitleVersion), i, 1, 1, 1);
		}

		QWidget* empty = new QWidget();
		empty->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
		p_wrapper_layout_seven->addWidget(empty);

	}

	QScrollArea *p_scroll_area_ADM = new QScrollArea;
	p_scroll_area_ADM->setWidgetResizable(true);
	p_wrapper_widget_seven->setLayout(p_wrapper_layout_seven);
	p_scroll_area_ADM->setWidget(p_wrapper_widget_seven);

	mpStackedLayout = new QStackedLayout();
	mpStackedLayout->insertWidget(WizardResourceGeneratorPage::WavIndex, p_wrapper_widget_two);
	mpStackedLayout->insertWidget(WizardResourceGeneratorPage::TTMLIndex, p_wrapper_widget_three);
	mpStackedLayout->insertWidget(WizardResourceGeneratorPage::Jpeg2000Index, p_wrapper_widget_four);
	mpStackedLayout->insertWidget(WizardResourceGeneratorPage::ExrIndex, p_wrapper_widget_four);
	mpStackedLayout->insertWidget(WizardResourceGeneratorPage::ISXDIndex, p_wrapper_widget_five);
	mpStackedLayout->insertWidget(WizardResourceGeneratorPage::MGAIndex, p_scroll_area_SADM);
	mpStackedLayout->insertWidget(WizardResourceGeneratorPage::ADMIndex, p_scroll_area_ADM);
	setLayout(mpStackedLayout);

	registerField(FIELD_NAME_SELECTED_FILES"*", this, "FilesSelected", SIGNAL(FilesListChanged()));
	registerField(FIELD_NAME_SOUNDFIELD_GROUP, this, "SoundfieldGroupSelected", SIGNAL(SoundfieldGroupChanged()));
	registerField(FIELD_NAME_EDIT_RATE, this, "EditRateSelected", SIGNAL(EditRateChanged()));
	registerField(FIELD_NAME_DURATION, this, "DurationSelected", SIGNAL(DurationChanged()));
	//WR
	registerField(FIELD_NAME_LANGUAGETAG_WAV, this, "LanguageTagWavSelected", SIGNAL(LanguageTagWavChanged()));
	registerField(FIELD_NAME_LANGUAGETAG_TT, this, "LanguageTagTTSelected", SIGNAL(LanguageTagTTChanged()));
	registerField(FIELD_NAME_MCA_TITLE, this, "MCATitleSelected", SIGNAL(MCATitleChanged()));
	registerField(FIELD_NAME_MCA_TITLE_VERSION, this, "MCATitleVersionSelected", SIGNAL(MCATitleVersionChanged()));
	registerField(FIELD_NAME_MCA_AUDIO_CONTENT_KIND, this, "MCAAudioContentKindSelected", SIGNAL(MCAAudioContentKindChanged()));
	registerField(FIELD_NAME_MCA_AUDIO_ELEMENT_KIND, this, "MCAAudioElementKindSelected", SIGNAL(MCAAudioElementKindChanged()));
	registerField(FIELD_NAME_CPL_EDIT_RATE, this, "CplEditRateSelected", SIGNAL(CplEditRateChanged()));
	registerField(FIELD_NAME_NAMESPACE_URI, this, "NamespaceURISelected", SIGNAL(NamespaceURIChanged()));
	//WR

	connect(mpFileDialog, SIGNAL(filesSelected(const QStringList &)), this, SLOT(SetSourceFiles(const QStringList &)));
	connect(mpComboBoxSoundfieldGroup, SIGNAL(currentTextChanged(const QString &)), this, SLOT(ChangeSoundfieldGroup(const QString&)));
	connect(mpSoundFieldGroupModel, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &, const QVector<int>&)), this, SIGNAL(completeChanged()));
}

//set "Generate" button enabled if all necessary values are edited
void WizardResourceGeneratorPage::textChanged()
{
	if (mpLineEditDurationEmptyTTGenerator->text().toInt() > 0 ){
		mpGenerateEmpty_button->setEnabled(true);
	}
	else
		mpGenerateEmpty_button->setEnabled(false);
}
//WR
void WizardResourceGeneratorPage::languageTagWavChanged()
{
	static QRegularExpression rx_lang("[a-z]{2,3}\\-([A-Z]{2}|[0-9]{3})");
	// ^$ empty string
	//qDebug() << mpLineEditLanguageTagWav->text();
	// TO-DO: Disable QWizard::FinishButton if string doesn't match regexp
}
void WizardResourceGeneratorPage::languageTagTTChanged()
{
	static QRegularExpression rx_lang("[a-z]{2,3}\\-([A-Z]{2}|[0-9]{3})");
	// ^$ empty string
	//qDebug() << mpLineEditLanguageTagWav->text();
	// TO-DO: Disable QWizard::FinishButton if string doesn't match regexp
	//rx_lang.exactMatch(mpLineEditLanguageTagTT->text())
}
//WR


//hide/show Generate Empty context, when "Generate Empty Timed Text Resource" button is clicked
void WizardResourceGeneratorPage::hideGroupBox() {

	if (mGroupBoxCheck == 0) {
		mpGroupBox->show();
		mGroupBoxCheck = 1;
	}
	else {
		mpGroupBox->hide();
		mGroupBoxCheck = 0;
	}
}

void WizardResourceGeneratorPage::GenerateEmptyTimedText(){

	Error error;
	//QStringList filePath(tr("%1/%2.xml").arg(mpLineEditFileDir->text()).arg(mpLineEditFileName->text()));
	QStringList filePath(QString("%1/%2").arg(AUX_FILES, "TTML_Empty_Minimal.xml"));
	if (!QFileInfo::exists(filePath.at(0))) error =  Error(Error::SourceFileOpenError, filePath.at(0));
	if(error.IsError() == true) {
		QString error_string = error.GetErrorDescription();
		mpMsgBox->setText(tr("Source File Error"));
		mpMsgBox->setInformativeText(error.GetErrorMsg()+error_string);
		mpMsgBox->setStandardButtons(QMessageBox::Ok);
		mpMsgBox->setDefaultButton(QMessageBox::Ok);
		mpMsgBox->setIcon(QMessageBox::Critical);
		mpMsgBox->exec();
		return;
	}


	QString dur = mpLineEditDurationEmptyTTGenerator->text();

	mpTimedTextModel->SetFile(filePath);
	mpTableViewTimedText->resizeRowsToContents();
	mpTableViewTimedText->resizeColumnsToContents();
	SwitchMode(WizardResourceGenerator::TTMLMode);
	emit FilesListChanged();
	QString profile;
	this->SetDuration(Duration(dur.toInt()));
	while(profile.isEmpty()) {

		int ret = mpSelectNamespaceDialog->exec();
		switch (ret) {
			case QDialog::Accepted:
				profile = mpComboBoxNamespaceURI->currentText();
				this->SetNamespaceURI(profile);
				break;
			case QDialog::Rejected:
				return;
		}
	}
	mpGroupBox->hide();
	mGroupBoxCheck = 0;
	mpLineEditFileDir->clear();
	mpLineEditDurationEmptyTTGenerator->clear();
	mpLineEditFileName->clear();
}

void WizardResourceGeneratorPage::SetSourceFiles(const QStringList &rFiles) {

	mpFileDialog->hide();
	mpComboBoxEditRate->setEnabled(true);
	if(rFiles.isEmpty() == false) {
		if(is_wav_file(rFiles.at(0))) {
			// Check sampling rate and bit depth consistence.
			Metadata metadata;
			mpAs02Wrapper->ReadMetadata(metadata, rFiles.at(0));
			for(int i = 0; i < rFiles.size(); i++) {
				Metadata other_metadata;
				mpAs02Wrapper->ReadMetadata(other_metadata, rFiles.at(i));
				if(other_metadata.editRate != EditRate::EditRate48000 && other_metadata.editRate != EditRate::EditRate96000) {
					mpMsgBox->setText(tr("Unsupported Sampling Rate"));
					mpMsgBox->setInformativeText(tr("%1 (%2 Hz). Only 48000 Hz and 96000 Hz are supported.").arg(other_metadata.fileName).arg(other_metadata.editRate.GetQuotient()));
					mpMsgBox->setStandardButtons(QMessageBox::Ok);
					mpMsgBox->setDefaultButton(QMessageBox::Ok);
					mpMsgBox->exec();
					return;
				}
				if(other_metadata.audioQuantization != 24) {
					mpMsgBox->setText(tr("Unsupported Bit Depth"));
					mpMsgBox->setInformativeText(tr("%1 (%2 bit). Only 24 bit are supported.").arg(other_metadata.fileName).arg(other_metadata.audioQuantization));
					mpMsgBox->setStandardButtons(QMessageBox::Ok);
					mpMsgBox->setDefaultButton(QMessageBox::Ok);
					mpMsgBox->exec();
					return;
				}
				if(other_metadata.editRate != metadata.editRate) {
					mpMsgBox->setText(tr("Sampling Rate mismatch"));
					mpMsgBox->setInformativeText(tr("Mismatch between %1 (%2 Hz) and %3 (%4 Hz)").arg(metadata.fileName).arg(metadata.editRate.GetQuotient()).arg(other_metadata.fileName).arg(other_metadata.editRate.GetQuotient()));
					mpMsgBox->setStandardButtons(QMessageBox::Ok);
					mpMsgBox->setDefaultButton(QMessageBox::Ok);
					mpMsgBox->exec();
					return;
				}
				if(other_metadata.duration != metadata.duration) {
					mpMsgBox->setText(tr("Duration mismatch"));
					mpMsgBox->setInformativeText(tr("Mismatch between %1 (%2) and %3 (%4)").arg(metadata.fileName).arg(metadata.duration.GetAsString(metadata.editRate)).arg(other_metadata.fileName).arg(other_metadata.duration.GetAsString(other_metadata.editRate)));
					mpMsgBox->setStandardButtons(QMessageBox::Ok);
					mpMsgBox->setDefaultButton(QMessageBox::Ok);
					mpMsgBox->exec();
					return;
				}
			}
			for(int i = 0; i < mpSoundFieldGroupModel->rowCount(); i++) mpTableViewWav->closePersistentEditor(mpSoundFieldGroupModel->index(i, SoundFieldGroupModel::ColumnDstChannel));
			mpSoundFieldGroupModel->SetFilesList(rFiles);
			for(int i = 0; i < mpSoundFieldGroupModel->rowCount(); i++) mpTableViewWav->openPersistentEditor(mpSoundFieldGroupModel->index(i, SoundFieldGroupModel::ColumnDstChannel));
			SwitchMode(WizardResourceGenerator::WavMode);
			emit FilesListChanged();
		}


			/* -----Denis Manthey----- */

		else if(is_ttml_file(rFiles.at(0))) {

			Metadata metadata;
			Error error;

			mpAs02Wrapper->SetCplEditRate(GetCplEditRate());
			error = mpAs02Wrapper->ReadMetadata(metadata, rFiles.at(0));

			if (!field(FIELD_NAME_NAMESPACE_URI).toString().isEmpty()) {
				metadata.profile = field(FIELD_NAME_NAMESPACE_URI).toString();
			}
			this->SetNamespaceURI(metadata.profile);
			//qvariant_cast<Duration>(p_resource_generator->field(FIELD_NAME_DURATION))
			if (mTimedTextDuration.GetCount() > 0) {
				metadata.duration = mTimedTextDuration;
				this->SetDuration(metadata.duration);
			}

			if(error.IsError()){
				mpMsgBox->setText(error.GetErrorDescription());
				mpMsgBox->setInformativeText(error.GetErrorMsg());
				mpMsgBox->setStandardButtons(QMessageBox::Ok);
				mpMsgBox->setDefaultButton(QMessageBox::Ok);
				mpMsgBox->exec();
				return;
			}

			if(!metadata.editRate.IsValid()) {
				mpMsgBox->setText(tr("No Edit Rate selected"));
				mpMsgBox->setInformativeText(tr("Select an Edit Rate for the Track File.\nIt is usually set to the Composition Edit Rate"));
				mpMsgBox->setStandardButtons(QMessageBox::Ok);
				mpMsgBox->setDefaultButton(QMessageBox::Ok);
				mpMsgBox->exec();
				//return;
			}

			while(metadata.duration.GetCount() == 0) {

				int ret = mpEditDurationDialog->exec();
				switch (ret) {
					case QDialog::Accepted:
						metadata.duration = Duration(mpLineEditDuration->text().toInt());
						break;
					case QDialog::Rejected:
						return;
				}
			}
			SetDuration(metadata.duration);

			while(metadata.profile.isEmpty()) {

				int ret = mpSelectNamespaceDialog->exec();
				switch (ret) {
					case QDialog::Accepted:
						metadata.profile = mpComboBoxNamespaceURI->currentText();
						this->SetNamespaceURI(metadata.profile);
						break;
					case QDialog::Rejected:
						return;
				}
			}

			SwitchMode(WizardResourceGenerator::TTMLMode);
			mpTimedTextModel->SetFile(rFiles);
			mpTableViewTimedText->resizeRowsToContents();
			mpTableViewTimedText->resizeColumnsToContents();
			emit FilesListChanged();
		}
		/* -----Denis Manthey----- */
		else if(is_xml_directory(rFiles.at(0))) {
			Metadata metadata;
			Error error;

			mpAs02Wrapper->SetCplEditRate(GetCplEditRate());
			error = mpAs02Wrapper->ReadMetadata(metadata, rFiles.at(0)); //QDir(rFiles.at(0)).entryList(QStringList("*.xml")).first());

			if (!field(FIELD_NAME_NAMESPACE_URI).toString().isEmpty()) {
				metadata.profile = field(FIELD_NAME_NAMESPACE_URI).toString();
			}
			this->SetNamespaceURI(metadata.profile);
			//qvariant_cast<Duration>(p_resource_generator->field(FIELD_NAME_DURATION))
			if (mTimedTextDuration.GetCount() > 0) {
				metadata.duration = mTimedTextDuration;
				this->SetDuration(metadata.duration);
			}

			if(error.IsError()){
				mpMsgBox->setText(error.GetErrorDescription());
				mpMsgBox->setInformativeText(error.GetErrorMsg());
				mpMsgBox->setStandardButtons(QMessageBox::Ok);
				mpMsgBox->setDefaultButton(QMessageBox::Ok);
				mpMsgBox->exec();
				return;
			}

			if(!metadata.editRate.IsValid()) {
				mpMsgBox->setText(tr("No Edit Rate selected"));
				mpMsgBox->setInformativeText(tr("Select an Edit Rate for the Track File.\nIt is usually set to the Composition Edit Rate"));
				mpMsgBox->setStandardButtons(QMessageBox::Ok);
				mpMsgBox->setDefaultButton(QMessageBox::Ok);
				mpMsgBox->exec();
				//return;
			}

			while(metadata.duration.GetCount() == 0) {

				int ret = mpEditDurationDialog->exec();
				switch (ret) {
					case QDialog::Accepted:
						metadata.duration = Duration(mpLineEditDuration->text().toInt());
						break;
					case QDialog::Rejected:
						return;
				}
			}
			SetDuration(metadata.duration);

			while(metadata.profile.isEmpty()) {

				int ret = mpSelectNamespaceDialog->exec();
				switch (ret) {
					case QDialog::Accepted:
						metadata.profile = mpComboBoxNamespaceURI->currentText();
						this->SetNamespaceURI(metadata.profile);
						break;
					case QDialog::Rejected:
						return;
				}
			}

			SwitchMode(WizardResourceGenerator::ISXDMode);
			mpTimedTextModel->SetFile(rFiles);
			mpTableViewTimedText->resizeRowsToContents();
			mpTableViewTimedText->resizeColumnsToContents();
			emit FilesListChanged();
		}

	}
}


QStringList WizardResourceGeneratorPage::GetFilesList() const {

	if(mpStackedLayout->currentIndex() == WizardResourceGeneratorPage::WavIndex) {
		return mpSoundFieldGroupModel->GetSourceFiles();
	}



		/* -----Denis Manthey----- */

	else if(mpStackedLayout->currentIndex() == WizardResourceGeneratorPage::TTMLIndex) {
		return mpTimedTextModel->GetSourceFile();
	}
		/* -----Denis Manthey----- */


	return QStringList();
}

void WizardResourceGeneratorPage::ShowFileDialog() {

	mpFileDialog->show();
}

void WizardResourceGeneratorPage::ShowDirDialog() {

	mpDirDialog->show();
}

SoundfieldGroup WizardResourceGeneratorPage::GetSoundfieldGroup() const {

	if(mpStackedLayout->currentIndex() == WizardResourceGeneratorPage::WavIndex) {
		return mpSoundFieldGroupModel->GetSoundfieldGroup();
	}
	return SoundfieldGroup();
}

void WizardResourceGeneratorPage::SetSoundfieldGroup(const SoundfieldGroup &rSoundfieldGroup) {

	for(int i = 0; i < mpSoundFieldGroupModel->rowCount(); i++) mpTableViewWav->closePersistentEditor(mpSoundFieldGroupModel->index(i, SoundFieldGroupModel::ColumnDstChannel));
	mpComboBoxSoundfieldGroup->setCurrentText(rSoundfieldGroup.GetName());
	mpSoundFieldGroupModel->SetSoundfieldGroup(rSoundfieldGroup);
	for(int i = 0; i < mpSoundFieldGroupModel->rowCount(); i++) mpTableViewWav->openPersistentEditor(mpSoundFieldGroupModel->index(i, SoundFieldGroupModel::ColumnDstChannel));
	emit SoundfieldGroupChanged();
	emit completeChanged();
}

void WizardResourceGeneratorPage::ChangeSoundfieldGroup(const QString &rName) {

	for(int i = 0; i < mpSoundFieldGroupModel->rowCount(); i++) mpTableViewWav->closePersistentEditor(mpSoundFieldGroupModel->index(i, SoundFieldGroupModel::ColumnDstChannel));
	mpSoundFieldGroupModel->ChangeSoundfieldGroup(rName);
	for(int i = 0; i < mpSoundFieldGroupModel->rowCount(); i++) mpTableViewWav->openPersistentEditor(mpSoundFieldGroupModel->index(i, SoundFieldGroupModel::ColumnDstChannel));
	emit completeChanged();
}

void WizardResourceGeneratorPage::SetEditRate(const EditRate &rEditRate) {

	mpComboBoxEditRate->setCurrentText(rEditRate.GetName());
	emit EditRateChanged();
	emit completeChanged();
}

void WizardResourceGeneratorPage::SetDuration(const Duration &rDuration) {
	mTimedTextDuration = rDuration;
	mpLineEditDurationReadOnlyTT->setText(QString::number(rDuration.GetCount()));
	mpLineEditDurationReadOnlyISXD->setText(QString::number(rDuration.GetCount()));
	emit DurationChanged();
	emit completeChanged();
}

//WR
void WizardResourceGeneratorPage::SetLanguageTagWav(const QString &rLanguageTag) {
	mpLineEditLanguageTagWav->setText(rLanguageTag);
	emit LanguageTagWavChanged();
	emit completeChanged();
}

void WizardResourceGeneratorPage::SetLanguageTagTT(const QString &rLanguageTag) {
	mpLineEditLanguageTagTT->setText(rLanguageTag);
	emit LanguageTagTTChanged();
	emit completeChanged();
}

void WizardResourceGeneratorPage::SetMCATitle(const QString &text) {
	mpLineEditMCATitle->setText(text);
	emit MCATitleChanged();
	emit completeChanged();
}

void WizardResourceGeneratorPage::SetMCATitleVersion(const QString &text) {
	mpLineEditMCATitleVersion->setText(text);
	emit MCATitleVersionChanged();
	emit completeChanged();
}

void WizardResourceGeneratorPage::SetMCAAudioContentKind(const QString &text) {
	mpLineEditMCAAudioContentKind->setEditText(text);
	emit MCAAudioContentKindChanged();
	emit completeChanged();
}

void WizardResourceGeneratorPage::SetMCAAudioElementKind(const QString &text) {
	mpLineEditMCAAudioElementKind->setEditText(text);
	emit MCAAudioElementKindChanged();
	emit completeChanged();
}

void WizardResourceGeneratorPage::SetCplEditRate(const EditRate &rEditRate) {
	mpComboBoxCplEditRateTT->setCurrentText(rEditRate.GetName());
	mpComboBoxCplEditRateISXD->setCurrentText(rEditRate.GetName());
	emit CplEditRateChanged();
	emit completeChanged();
}

void WizardResourceGeneratorPage::SetNamespaceURI(const QString &text) {
	mpLineEditNamespaceURITT->setText(text);
	mpLineEditNamespaceURIISXD->setText(text);
	emit NamespaceURIChanged();
	emit completeChanged();
}
//WR


EditRate WizardResourceGeneratorPage::GetEditRate() const {

	return EditRate::GetEditRate(mpComboBoxEditRate->currentText());
}

Duration WizardResourceGeneratorPage::GetDuration() const {

	return Duration(mpLineEditDurationReadOnlyTT->text().toInt());
}

//WR
QString WizardResourceGeneratorPage::GetLanguageTagWav() const {

	return mpLineEditLanguageTagWav->text();
}

QString WizardResourceGeneratorPage::GetLanguageTagTT() const {

	return mpLineEditLanguageTagTT->text();
}

QString WizardResourceGeneratorPage::GetMCATitle() const {

	return mpLineEditMCATitle->text();
}

QString WizardResourceGeneratorPage::GetMCATitleVersion() const {

	return mpLineEditMCATitleVersion->text();
}

QString WizardResourceGeneratorPage::GetMCAAudioContentKind() const {

	return mpLineEditMCAAudioContentKind->currentText();
}

QString WizardResourceGeneratorPage::GetMCAAudioElementKind() const {

	return mpLineEditMCAAudioElementKind->currentText();
}
//WR

EditRate WizardResourceGeneratorPage::GetCplEditRate() const {

	return EditRate::GetEditRate(mpComboBoxCplEditRateTT->currentText());
}

QString WizardResourceGeneratorPage::GetNamespaceURI() const {

	return mpLineEditNamespaceURITT->text();
}



bool WizardResourceGeneratorPage::isComplete() const {

	bool are_mandatory_fields_filled = QWizardPage::isComplete();
	if(mpStackedLayout->currentIndex() == WizardResourceGeneratorPage::ExrIndex) {
		return are_mandatory_fields_filled && GetEditRate().IsValid();
	}
	else if(mpStackedLayout->currentIndex() == WizardResourceGeneratorPage::WavIndex) {
		return are_mandatory_fields_filled && GetSoundfieldGroup().IsComplete();
	}
	else if(mpStackedLayout->currentIndex() == WizardResourceGeneratorPage::TTMLIndex) {
		return are_mandatory_fields_filled;
	}
	return false;
}

void WizardResourceGeneratorPage::SwitchMode(WizardResourceGenerator::eMode mode) {

	switch(mode) {
		case WizardResourceGenerator::ExrMode:
			mpStackedLayout->setCurrentIndex(ExrIndex);
			break;
		case WizardResourceGenerator::WavMode:
			mpStackedLayout->setCurrentIndex(WavIndex);
			mpFileDialog->setOption(QFileDialog::ShowDirsOnly, false);
			mpFileDialog->setNameFilters(QStringList() << "*.wav" );
			mpFileDialog->setFileMode(QFileDialog::ExistingFiles);
			break;

				/* -----Denis Manthey----- */
		case WizardResourceGenerator::TTMLMode:
			mpStackedLayout->setCurrentIndex(TTMLIndex);
			mpFileDialog->setOption(QFileDialog::ShowDirsOnly, false);
			mpFileDialog->setNameFilters(QStringList() << "*.ttml *.xml");
			mpFileDialog->setFileMode(QFileDialog::ExistingFile);
			break;
				/* -----Denis Manthey----- */

		case WizardResourceGenerator::Jpeg2000Mode:
			mpStackedLayout->setCurrentIndex(Jpeg2000Index);
			break;

		case WizardResourceGenerator::ISXDMode:
			mpStackedLayout->setCurrentIndex(ISXDIndex);
			mpFileDialog->setOption(QFileDialog::ShowDirsOnly, true);
			mpFileDialog->setFileMode(QFileDialog::Directory);
			break;

		case WizardResourceGenerator::MGAMode:
			mpStackedLayout->setCurrentIndex(MGAIndex);
			break;

		case WizardResourceGenerator::ADMMode:
			mpStackedLayout->setCurrentIndex(ADMIndex);
			break;

		default:
			break;
	}
}

WidgetProxyImage::WidgetProxyImage(QWidget *pParent /*= NULL*/) :
QWidget(NULL), mIndex(QPersistentModelIndex()), mpSpinner(NULL), mpTimer(NULL), mpImageLabel(NULL) {

	setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowTransparentForInput);
	mpTimer = new QTimer(this);
	mpTimer->setSingleShot(true);
	setGeometry(0, 0, 100, 100);
	InitLayout();
}

void WidgetProxyImage::rTimeout() {

	show();
	QRect rect = geometry();
	rect.moveTopLeft(QCursor::pos());
	setGeometry(rect);
	mpSpinner->move(QRect(QPoint(0, 0), geometry().size()).center() - mpSpinner->rect().center());
}

void WidgetProxyImage::InitLayout() {

	setObjectName("ProxyImage");
	setStyleSheet(QString(
		"QWidget#%1 {"
		"background-color: black;"
		"}").arg(objectName()));
	mpSpinner = new QtWaitingSpinner(this);
	mpSpinner->setMinimumTrailOpacity(15.0);
	mpSpinner->setTrailFadePercentage(70.0);
	mpSpinner->setNumberOfLines(15);
	mpSpinner->setLineLength(15);
	mpSpinner->setLineWidth(5);
	mpSpinner->setInnerRadius(15);
	mpSpinner->setRevolutionsPerSecond(2);
	mpSpinner->setColor(QColor(255, 255, 255));

	mpImageLabel = new QLabel(this);

	QHBoxLayout *p_layout = new QHBoxLayout();
	p_layout->setSpacing(0);
	p_layout->setContentsMargins(0, 0, 0, 0);
	p_layout->addWidget(mpImageLabel);
	setLayout(p_layout);
}


void WidgetProxyImage::rTransformationFinished(const QImage &rImage, const QVariant &rIdentifier) {

	if(mpImageLabel->pixmap().isNull() && rIdentifier.toModelIndex() == mIndex) {
		mpSpinner->stop();
		mpImageLabel->setPixmap(QPixmap::fromImage(rImage));
		QRect rect = geometry();
		rect.setWidth(rImage.width());
		rect.setHeight(rImage.height());
		setGeometry(rect);
	}
}

void WidgetProxyImage::hide() {

	disconnect(mpTimer, NULL, NULL, NULL);
	mpImageLabel->clear();
	repaint();
	QWidget::hide();
}


SoundFieldGroupModel::SoundFieldGroupModel(QObject *pParent /*= NULL*/) :
QAbstractTableModel(pParent), mSourceFilesChannels(QList<QPair<QString, unsigned int> >()), mSoundfieldGroup(SoundfieldGroup::SoundFieldGroupNone), mpAs02Wrapper(NULL) {

	mpAs02Wrapper = new MetadataExtractor(this);
}

void SoundFieldGroupModel::SetFilesList(const QStringList &rSourceFile) {

	beginResetModel();
	mSourceFilesChannels.clear();
	for(int i = 0; i < rSourceFile.size(); i++) {
		Metadata metadata;
		Error error = mpAs02Wrapper->ReadMetadata(metadata, rSourceFile.at(i));
		if(error.IsError() == false) {
			for(unsigned int ii = 0; ii < metadata.audioChannelCount; ii++) {
				mSourceFilesChannels.push_back(QPair<QString, unsigned int>(rSourceFile.at(i), ii)); // Add entry for every channel.
			}
		}
		else {
			qWarning() << error;
		}
	}
	endResetModel();
}

QStringList SoundFieldGroupModel::GetSourceFiles() const {

	QStringList ret;
	for(int i = 0; i < mSourceFilesChannels.size(); i++) {
		if(mSourceFilesChannels.at(i).second == 0)ret.push_back(mSourceFilesChannels.at(i).first);
	}
	return ret;
}

Qt::ItemFlags SoundFieldGroupModel::flags(const QModelIndex &rIndex) const {

	const int row = rIndex.row();
	const int column = rIndex.column();

	if(column == SoundFieldGroupModel::ColumnDstChannel) {
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
	}
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

int SoundFieldGroupModel::rowCount(const QModelIndex &rParent /*= QModelIndex()*/) const {

	if(rParent.isValid() == false) {
		return mSourceFilesChannels.size();
	}
	return 0;
}

int SoundFieldGroupModel::columnCount(const QModelIndex &rParent /*= QModelIndex()*/) const {

	if(rParent.isValid() == false) {
		return ColumnMax;
	}
	return 0;
}

QVariant SoundFieldGroupModel::data(const QModelIndex &rIndex, int role /*= Qt::DisplayRole*/) const {

	const int row = rIndex.row();
	const int column = rIndex.column();

	if(row < mSourceFilesChannels.size()) {
		if(column == SoundFieldGroupModel::ColumnIcon) {
			// icon
			if(role == Qt::DecorationRole) {
				return QVariant(QPixmap(":/sound_small.png"));
			}
		}
		else if(column == SoundFieldGroupModel::ColumnSourceFile) {
			if(role == Qt::DisplayRole) {
				return QVariant(QString("[%1]").arg(get_file_name(mSourceFilesChannels.at(row).first)));
			}
			else if(role == Qt::ToolTipRole) {
				Metadata metadata;
				Error error = mpAs02Wrapper->ReadMetadata(metadata, mSourceFilesChannels.at(row).first);
				if(error.IsError() == true) {
					return QVariant(error.GetErrorMsg());
				}
				return QVariant(metadata.GetAsString());
			}
		}
		else if(column == SoundFieldGroupModel::ColumnSrcChannel) {
			if(role == Qt::DisplayRole) {
				return QVariant(tr("Channel %1 maps to: ").arg(mSourceFilesChannels.at(row).second));
			}
		}
		else if(column == SoundFieldGroupModel::ColumnDstChannel) {
			if(role == UserRoleComboBox) {
				return QVariant(mSoundfieldGroup.GetAdmittedChannelNames());
			}
			else if(role == Qt::DisplayRole) {
				return QVariant(mSoundfieldGroup.GetChannelName(row));
			}
		}
	}
	return QVariant();
}

bool SoundFieldGroupModel::setData(const QModelIndex &rIndex, const QVariant &rValue, int role /*= Qt::EditRole*/) {

	const int row = rIndex.row();
	const int column = rIndex.column();

	if(row < mSourceFilesChannels.size()) {
		if(column == SoundFieldGroupModel::ColumnDstChannel) {
			if(role == Qt::EditRole) {
				bool success = mSoundfieldGroup.AddChannel(row, rValue.toString());
				if(success) {
					emit dataChanged(rIndex, rIndex);
					return true;
				}
			}
		}
	}
	return false;
}

void SoundFieldGroupModel::ChangeSoundfieldGroup(const QString &rName) {

	beginResetModel();
	mSoundfieldGroup = SoundfieldGroup::GetSoundFieldGroup(rName);
	endResetModel();
}

void SoundFieldGroupModel::SetSoundfieldGroup(const SoundfieldGroup &rSoundfieldGroup) {

	beginResetModel();
	mSoundfieldGroup = rSoundfieldGroup;
	endResetModel();
}



			/* -----Denis Manthey----- */

TimedTextModel::TimedTextModel(QObject *pParent /*= NULL*/) :
QAbstractTableModel(pParent), mpAs02Wrapper(NULL) {

	mpAs02Wrapper = new MetadataExtractor(this);
}

void TimedTextModel::SetFile(const QStringList &rSourceFile) {

	beginResetModel();
	mSelectedFile.clear();
	mSelectedFile = rSourceFile;
	endResetModel();
}
int TimedTextModel::rowCount(const QModelIndex &rParent /*= QModelIndex()*/) const {

	if(rParent.isValid() == false) {
		return mSelectedFile.size();
	}
	return 0;
}
int TimedTextModel::columnCount(const QModelIndex &rParent /*= QModelIndex()*/) const {

	if(rParent.isValid() == false) {
		return ColumnMax;
	}
	return 0;
}
QVariant TimedTextModel::data(const QModelIndex &rIndex, int role /*= Qt::DisplayRole*/) const {

	const int row = rIndex.row();
	const int column = rIndex.column();
	if(row < mSelectedFile.size()) {
		if(column == TimedTextModel::ColumnIcon) {
			if(role == Qt::DecorationRole) {
				return QVariant(QPixmap(":/text_small.png"));
			}
		}
		else if(column == TimedTextModel::ColumnFilePath) {
			if(role == Qt::DisplayRole) {
				return QVariant(mSelectedFile.at(row));
			}
			else if(role == Qt::ToolTipRole) {
				Metadata metadata;
				Error error = mpAs02Wrapper->ReadMetadata(metadata, mSelectedFile.at(row));
				if(error.IsError() == true) {
					return QVariant(error.GetErrorMsg());
				}
				return QVariant(metadata.GetAsString());
			}
		}
	}
	return QVariant();
}
			/* -----Denis Manthey----- */
