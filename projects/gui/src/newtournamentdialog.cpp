/*
    This file is part of Cute Chess.

    Cute Chess is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Cute Chess is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Cute Chess.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "newtournamentdialog.h"
#include "ui_newtournamentdlg.h"

#include <QFileDialog>
#include <QSettings>
#include <QMenu>
#include <algorithm>

#include <board/boardfactory.h>
#include <enginemanager.h>
#include <enginebuilder.h>
#include <timecontrol.h>
#include <tournament.h>
#include <tournamentfactory.h>
#include <openingsuite.h>
#include <polyglotbook.h>
#include <mersenne.h>
#include <QRandomGenerator>

#include "engineconfigurationmodel.h"
#include "engineconfigproxymodel.h"
#include "engineconfigurationdlg.h"
#include "timecontroldlg.h"
#include "engineselectiondlg.h"

#if 0
#include <modeltest.h>
#endif

NewTournamentDialog::NewTournamentDialog(EngineManager* engineManager,
					 QWidget *parent)
	: QDialog(parent),
	  m_srcEngineManager(engineManager),
	  ui(new Ui::NewTournamentDialog)
{
	Q_ASSERT(engineManager != nullptr);
	ui->setupUi(this);

	m_srcEnginesModel = new EngineConfigurationModel(engineManager, this);
	#if 0
	new ModelTest(m_srcEnginesModel, this);
	#endif

	m_proxyModel = new EngineConfigurationProxyModel(this);
	m_proxyModel->setSourceModel(m_srcEnginesModel);
	m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
	m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_proxyModel->sort(0);
	m_proxyModel->setDynamicSortFilter(true);

	connect(ui->m_gameSettings, SIGNAL(variantChanged(QString)),
		this, SLOT(onVariantChanged(QString)));

	connect(ui->m_addEngineBtn, SIGNAL(clicked()),
		this, SLOT(addEngine()));
	connect(ui->m_removeEngineBtn, SIGNAL(clicked()),
		this, SLOT(removeEngine()));
	connect(ui->m_configureEngineBtn, &QToolButton::clicked, [=]()
	{
		configureEngine(ui->m_playersList->currentIndex());
	});
	connect(ui->m_moveEngineUpBtn, &QToolButton::clicked, [=]()
	{
		moveEngine(-1);
	});
	connect(ui->m_moveEngineDownBtn, &QToolButton::clicked, [=]()
	{
		moveEngine(1);
	});

	connect(ui->m_browsePgnoutBtn, &QPushButton::clicked, this, [=]()
	{
		auto dlg = new QFileDialog(this, tr("Select PGN output file"),
			QString(), tr("Portable Game Notation (*.pgn)"));
		connect(dlg, &QFileDialog::fileSelected, ui->m_pgnoutEdit, &QLineEdit::setText);
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		dlg->setAcceptMode(QFileDialog::AcceptSave);
		dlg->open();
	});
	connect(ui->m_browseEpdoutBtn, &QPushButton::clicked, this, [=]()
	{
		auto dlg = new QFileDialog(this, tr("Select EPD output file"),
			QString(), tr("Extended Position Description (*.epd)"));
		connect(dlg, &QFileDialog::fileSelected, ui->m_epdoutEdit, &QLineEdit::setText);
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		dlg->setAcceptMode(QFileDialog::AcceptSave);
		dlg->open();
	});

	m_addedEnginesManager = new EngineManager(this);
	m_addedEnginesModel = new EngineConfigurationModel(
		m_addedEnginesManager, this);
	ui->m_playersList->setModel(m_addedEnginesModel);

	connect(ui->m_playersList->selectionModel(),
		SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
		this, SLOT(onPlayerSelectionChanged(QItemSelection, QItemSelection)));
	connect(ui->m_playersList, SIGNAL(doubleClicked(QModelIndex)),
		this, SLOT(configureEngine(QModelIndex)));

	ui->m_playersList->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
	connect(ui->m_playersList, SIGNAL(customContextMenuRequested(const QPoint&)),
		this, SLOT(onContextMenuRequest()));

	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
	connect(ui->m_gameSettings, &GameSettingsWidget::statusChanged, [=](bool ok)
	{
		if (ok)
			ok = canStart();
		ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(ok);
	});

	ui->m_gameSettings->onHumanCountChanged(0);
	onVariantChanged(ui->m_gameSettings->chessVariant());
	readSettings();
}

NewTournamentDialog::~NewTournamentDialog()
{
	delete ui;
}

void NewTournamentDialog::addEngineOnDblClick(const QModelIndex& index)
{
	const QListView* listView = ((QListView*)sender());
	const QModelIndex& idx = m_proxyModel->mapToSource(index);

	m_addedEnginesManager->addEngine(m_srcEngineManager->engineAt(idx.row()));
	m_timeControls << TimeControl();
	listView->selectionModel()->select(index, QItemSelectionModel::Deselect);

	QPushButton* button = ui->buttonBox->button(QDialogButtonBox::Ok);
	button->setEnabled(canStart());
}


void NewTournamentDialog::addEngine()
{
	EngineSelectionDialog dlg(m_proxyModel);
	connect(dlg.enginesList(), SIGNAL(doubleClicked(QModelIndex)), this,
		SLOT(addEngineOnDblClick(QModelIndex)));
	int value = dlg.exec();
	disconnect(dlg.enginesList(), SIGNAL(doubleClicked(QModelIndex)), this,
		   SLOT(addEngineOnDblClick(QModelIndex)));

	if (value != QDialog::Accepted)
		return;

	const QModelIndexList list(dlg.selectedRows());
	for (const QModelIndex& index : list)
	{
		m_addedEnginesManager->addEngine(m_srcEngineManager->engineAt(index.row()));
		m_timeControls << TimeControl();
	}

	QPushButton* button = ui->buttonBox->button(QDialogButtonBox::Ok);
	button->setEnabled(canStart());
}

void NewTournamentDialog::removeEngine()
{
	QList<QModelIndex> selected = ui->m_playersList->selectionModel()->selectedRows();

	// Can't use std::greater because operator> isn't implemented
	// for QModelIndex.
	std::sort(selected.begin(), selected.end(),
	[](const QModelIndex &a, const QModelIndex &b)
	{
		return b < a;
	});

	for (const QModelIndex& index : std::as_const(selected))
	{
		m_addedEnginesManager->removeEngineAt(index.row());
		m_timeControls.remove(index.row());
	}

	QPushButton* button = ui->buttonBox->button(QDialogButtonBox::Ok);
	button->setEnabled(canStart());
}

void NewTournamentDialog::configureEngine(const QModelIndex& index)
{
	EngineConfigurationDialog dlg(EngineConfigurationDialog::ConfigureEngine);

	int row = index.row();
	const EngineConfiguration& config = m_addedEnginesManager->engineAt(row);
	dlg.applyEngineInformation(config);

	QSet<QString> names = m_addedEnginesManager->engineNames();
	names.remove(config.name());
	dlg.setReservedNames(names);

	if (dlg.exec() == QDialog::Accepted)
		m_addedEnginesManager->updateEngineAt(row, dlg.engineConfiguration());

	QPushButton* button = ui->buttonBox->button(QDialogButtonBox::Ok);
	button->setEnabled(canStart());
}

void NewTournamentDialog::moveEngine(int offset)
{
	if (offset == 0)
		return;

	QModelIndex index(ui->m_playersList->currentIndex());
	int row1 = index.row();
	int row2 = row1 + offset;

	// It should be impossible for either row to be out of bounds,
	// but we'll check it explicitly, which makes the compiler happy.
	if (row1 < 0 || row1 >= m_timeControls.size())
	{
		qWarning("row1 out of bounds");
		return;
	}
	if (row2 < 0 || row2 >= m_timeControls.size())
	{
		qWarning("row2 out of bounds");
		return;
	}

	EngineConfiguration tmp(m_addedEnginesManager->engineAt(row1));
	m_addedEnginesManager->updateEngineAt(row1, m_addedEnginesManager->engineAt(row2));
	m_addedEnginesManager->updateEngineAt(row2, tmp);

	m_timeControls.swapItemsAt(row1, row2);

	ui->m_playersList->setCurrentIndex(index.sibling(row2, 0));
}

bool NewTournamentDialog::canStart() const
{
	if (!ui->m_gameSettings->isValid())
		return false;

	if (m_addedEnginesManager->engineCount() < 2)
		return false;

	QPushButton* button = ui->buttonBox->button(QDialogButtonBox::Ok);

	// check for duplicate configuration names
	if (m_addedEnginesManager->engineNames().count()
	!=  m_addedEnginesManager->engineCount())
	{
		button->setText(tr("Resolve Duplicates!"));
		return false;
	}
	button->setText("&OK");

	QString variant = ui->m_gameSettings->chessVariant();
	return m_addedEnginesManager->supportsVariant(variant);
}

void NewTournamentDialog::onVariantChanged(const QString& variant)
{
	m_proxyModel->setFilterVariant(variant);
	ui->m_addEngineBtn->setEnabled(m_proxyModel->rowCount() > 0);

	m_addedEnginesModel->setChessVariant(variant);
	QPushButton* button = ui->buttonBox->button(QDialogButtonBox::Ok);
	button->setEnabled(canStart());

	onPlayerSelectionChanged(QItemSelection(), QItemSelection());
}

void NewTournamentDialog::onPlayerSelectionChanged(const QItemSelection& selected,
						   const QItemSelection& deselected)
{
	Q_UNUSED(selected);
	Q_UNUSED(deselected);

	bool enable = ui->m_playersList->selectionModel()->hasSelection();
	ui->m_configureEngineBtn->setEnabled(enable);
	ui->m_removeEngineBtn->setEnabled(enable);

	int i = ui->m_playersList->currentIndex().row();
	ui->m_moveEngineUpBtn->setEnabled(enable && i > 0);
	ui->m_moveEngineDownBtn->setEnabled(enable && i < m_addedEnginesManager->engineCount() - 1);
}

void NewTournamentDialog::onContextMenuRequest()
{
	QList<QModelIndex> selected = ui->m_playersList->selectionModel()->selectedRows();
	if (selected.isEmpty())
		return;

	QMenu menu(ui->m_playersList);

	auto editTimeControlAct = menu.addAction(tr("Edit Time Control"));
	connect(editTimeControlAct, &QAction::triggered, this, [=]()
	{
		int i = selected.first().row();
		TimeControl tc {m_timeControls.at(i)};
		if (!tc.isValid())
			tc = ui->m_gameSettings->timeControl();

		auto dlg = new TimeControlDialog(tc, this);
		QString name {m_addedEnginesManager->engines().at(i).name()};
		if (selected.count() > 1)
			name.append(tr(" - %0 engines").arg(selected.count()));
		dlg->setWindowTitle(tr("Time Control - %0").arg(name));

		if (dlg->exec() == QDialog::Accepted)
			for (const QModelIndex& index: selected)
				m_timeControls[index.row()] = dlg->timeControl();
		delete dlg;
	});

	menu.exec(QCursor::pos());
}

static QVariantMap timeControlToVariant(const TimeControl& tc)
{
	QVariantMap map;
	if (!tc.isValid())
		return map;

	map["infinite"] = tc.isInfinite();
	map["hourglass"] = tc.isHourglass();
	map["timePerTc"] = tc.timePerTc();
	map["movesPerTc"] = tc.movesPerTc();
	map["increment"] = tc.timeIncrement();
	map["timePerMove"] = tc.timePerMove();
	map["plyLimit"] = tc.plyLimit();
	map["nodeLimit"] = tc.nodeLimit();
	map["expiryMargin"] = tc.expiryMargin();
	return map;
}

static TimeControl timeControlFromVariant(const QVariantMap& map)
{
	TimeControl tc;
	if (map.isEmpty())
		return tc;

	tc.setInfinity(map.value("infinite").toBool());
	tc.setHourglass(map.value("hourglass").toBool());
	tc.setTimePerTc(map.value("timePerTc").toInt());
	tc.setMovesPerTc(map.value("movesPerTc").toInt());
	tc.setTimeIncrement(map.value("increment").toInt());
	tc.setTimePerMove(map.value("timePerMove").toInt());
	tc.setPlyLimit(map.value("plyLimit").toInt());
	tc.setNodeLimit(map.value("nodeLimit").toLongLong());
	tc.setExpiryMargin(map.value("expiryMargin").toInt());
	return tc;
}

Tournament* NewTournamentDialog::createTournament(GameManager* gameManager) const
{
	return createTournament(tournamentConfig(), gameManager, parent());
}

QVariantMap NewTournamentDialog::tournamentConfig() const
{
	auto ts = ui->m_tournamentSettings;
	QVariantMap config;

	config["type"] = ts->tournamentType();
	config["name"] = ui->m_nameEdit->text();
	config["site"] = ui->m_siteEdit->text();
	config["pgnOutput"] = ui->m_pgnoutEdit->text();
	config["epdOutput"] = ui->m_epdoutEdit->text();
	config["seedCount"] = ts->seedCount();
	config["gamesPerEncounter"] = ts->gamesPerEncounter();
	config["rounds"] = ts->rounds();
	config["startDelay"] = ts->delayBetweenGames();
	config["openingRepetitions"] = ts->openingRepetitions();
	config["recover"] = ts->engineRecovery();
	config["saveUnfinishedGames"] = ts->savingOfUnfinishedGames();
	config["swapSides"] = ts->swappingSides();
	config["reverseSides"] = ts->reversingSchedule();
	config["resultFormat"] = ts->resultFormat();

	// The random seed makes random opening order and knockout
	// pairings reproducible when the tournament is resumed.
	config["randomSeed"] = QRandomGenerator::global()->generate();

	config["games"] = ui->m_gameSettings->toVariant();
	config["timeControl"] = timeControlToVariant(
		ui->m_gameSettings->timeControl());

	QVariantList players;
	const auto engines = m_addedEnginesManager->engines();
	for (int i = 0; i < engines.count(); i++)
	{
		EngineConfiguration engine = engines.at(i);
		ui->m_gameSettings->applyEngineConfiguration(&engine);

		QVariantMap player;
		player["engine"] = engine.toVariant();
		player["timeControl"] = timeControlToVariant(m_timeControls.at(i));
		players << player;
	}
	config["players"] = players;

	return config;
}

Tournament* NewTournamentDialog::createTournament(const QVariantMap& config,
						  GameManager* gameManager,
						  QObject* parent,
						  QString* error)
{
	Q_ASSERT(gameManager != nullptr);

	auto setError = [=](const QString& message)
	{
		if (error != nullptr)
			*error = message;
	};

	const QVariantMap games = config.value("games").toMap();
	const QString variant = games.value("variant").toString();
	if (!Chess::BoardFactory::variants().contains(variant))
	{
		setError(tr("Unknown chess variant: %1").arg(variant));
		return nullptr;
	}

	const QVariantList players = config.value("players").toList();
	if (players.size() < 2)
	{
		setError(tr("The tournament needs at least two players"));
		return nullptr;
	}

	auto t = TournamentFactory::create(
		config.value("type").toString(), gameManager, parent);
	if (t == nullptr)
	{
		setError(tr("Unknown tournament type: %1")
			 .arg(config.value("type").toString()));
		return nullptr;
	}

	if (config.contains("randomSeed"))
		Mersenne::initialize(config.value("randomSeed").toUInt());

	t->setPgnCleanupEnabled(false);
	t->setName(config.value("name").toString());
	t->setSite(config.value("site").toString());
	t->setVariant(variant);
	t->setPgnOutput(config.value("pgnOutput").toString());
	t->setEpdOutput(config.value("epdOutput").toString());

	t->setSeedCount(config.value("seedCount").toInt());
	t->setGamesPerEncounter(qMax(1, config.value("gamesPerEncounter").toInt()));
	if (t->canSetRoundMultiplier())
		t->setRoundMultiplier(qMax(1, config.value("rounds").toInt()));
	t->setStartDelay(config.value("startDelay").toInt());

	const QVariantMap adj = games.value("adjudication").toMap();
	GameAdjudicator adjudicator;
	adjudicator.setDrawThreshold(adj.value("drawMoveNumber").toInt(),
				     adj.value("drawMoveCount").toInt(),
				     adj.value("drawScore").toInt());
	adjudicator.setResignThreshold(adj.value("resignMoveCount").toInt(),
				       adj.value("resignScore").toInt(),
				       adj.value("resignTwoSided").toBool());
	adjudicator.setMaximumGameLength(adj.value("maxGameLength").toInt());
	adjudicator.setTablebaseAdjudication(adj.value("tablebases").toBool());
	t->setAdjudicator(adjudicator);

	const QVariantMap suiteMap = games.value("openingSuite").toMap();
	OpeningSuite* suite = nullptr;
	const QString fen = suiteMap.value("fen").toString();
	const QString suiteFile = suiteMap.value("file").toString();
	if (!fen.isEmpty())
		suite = new OpeningSuite(fen);
	else if (!suiteFile.isEmpty())
	{
		auto format = OpeningSuite::PgnFormat;
		if (suiteFile.endsWith(".epd", Qt::CaseInsensitive))
			format = OpeningSuite::EpdFormat;
		auto order = OpeningSuite::SequentialOrder;
		if (suiteMap.value("randomOrder").toBool())
			order = OpeningSuite::RandomOrder;

		suite = new OpeningSuite(suiteFile, format, order, 0);
		if (!suite->initialize())
		{
			delete suite;
			suite = nullptr;
			qWarning("Could not open opening suite %s",
				 qUtf8Printable(suiteFile));
		}
	}
	t->setOpeningSuite(suite);
	t->setOpeningDepth(suiteMap.value("depth", 1).toInt());

	t->setOpeningBookOwnership(true);
	const QVariantMap bookMap = games.value("openingBook").toMap();
	OpeningBook* book = nullptr;
	const QString bookFile = bookMap.value("file").toString();
	if (!bookFile.isEmpty())
	{
		auto mode = bookMap.value("diskAccess").toBool()
			    ? OpeningBook::Disk : OpeningBook::Ram;
		book = new PolyglotBook(mode);
		if (!book->read(bookFile))
		{
			delete book;
			book = nullptr;
			qWarning("Could not open opening book %s",
				 qUtf8Printable(bookFile));
		}
	}
	int bookDepth = bookMap.value("depth", 10).toInt();

	t->setOpeningRepetitions(config.value("openingRepetitions", 1).toInt());
	t->setRecoveryMode(config.value("recover").toBool());
	t->setPgnWriteUnfinishedGames(
		config.value("saveUnfinishedGames", true).toBool());
	t->setSwapSides(config.value("swapSides", true).toBool());
	t->setReverseSides(config.value("reverseSides").toBool());
	t->setResultFormat(config.value("resultFormat").toString());

	const TimeControl defaultTc = timeControlFromVariant(
		config.value("timeControl").toMap());
	bool isHourglass = defaultTc.isHourglass();

	for (const QVariant& var : players)
	{
		const QVariantMap player = var.toMap();
		EngineConfiguration engine(player.value("engine"));
		TimeControl tc = timeControlFromVariant(
			player.value("timeControl").toMap());
		// Hourglass mode must be the same for all players
		tc.setHourglass(isHourglass);

		t->addPlayer(new EngineBuilder(engine),
			     tc.isValid() ? tc : defaultTc,
			     book,
			     bookDepth);
	}

	return t;
}

void NewTournamentDialog::readSettings()
{
	ui->m_siteEdit->setText(QSettings().value("pgn/site").toString());

	QString pgnName = ui->m_pgnoutEdit->text();
	if (pgnName.isEmpty())
	{
		pgnName = QSettings().value("tournament/default_pgn_output_file",
					    QString()).toString();
		ui->m_pgnoutEdit->setText(pgnName);
	}

	QString epdName = ui->m_epdoutEdit->text();
	if (epdName.isEmpty())
	{
		epdName = QSettings().value("tournament/default_epd_output_file",
					    QString()).toString();
		ui->m_epdoutEdit->setText(epdName);
	}
}
