/*
    This file is part of Cute Chess.
    Copyright (C) 2008-2018 Cute Chess authors

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

#include "tournamentresultswidget.h"

#include <limits>

#include <QPlainTextEdit>
#include <QBoxLayout>
#include <QFont>
#include <QScrollBar>

#include <tournament.h>

TournamentResultsWidget::TournamentResultsWidget(QWidget* parent)
	: QWidget(parent)
{
	m_resultsEdit = new QPlainTextEdit(this);
	m_resultsEdit->setReadOnly(true);
	m_resultsEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

	QFont font("Courier New");
	font.setStyleHint(QFont::Monospace);
	font.setPointSize(font.pointSize() - 1);
	m_resultsEdit->document()->setDefaultFont(font);

	auto layout = new QBoxLayout(QBoxLayout::TopToBottom);
	layout->addWidget(m_resultsEdit);
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);
}

void TournamentResultsWidget::setTournament(Tournament* tournament)
{
	if (m_tournament)
		m_tournament->disconnect(this);

	m_tournament = tournament;
	if (tournament == nullptr)
		return;

	connect(tournament, SIGNAL(gameFinished(ChessGame*, int, int, int)),
		this, SLOT(onTournamentChanged()));
	connect(tournament, SIGNAL(finished()),
		this, SLOT(onTournamentChanged()));
	m_resultsEdit->setPlainText(tournament->results());
}

void TournamentResultsWidget::onTournamentChanged()
{
	if (m_tournament)
		updateResults(m_tournament);
}

void TournamentResultsWidget::updateResults(Tournament* tournament)
{
	Q_ASSERT(tournament != nullptr);
	QString text;

	// A quick fix, copied from the CLI side.
	if (tournament->playerCount() == 2 && tournament->type() != "knockout")
	{
		TournamentPlayer fcp = tournament->playerAt(0);
		TournamentPlayer scp = tournament->playerAt(1);
		int totalResults = fcp.gamesFinished();
		double scoreRatio = std::numeric_limits<double>::quiet_NaN();
		if (totalResults > 0)
			scoreRatio = double(fcp.score()) / (totalResults * 2);
		text = tr("Score of %1 vs %2: %3 - %4 - %5 [%6]\n") // clazy:exclude=qstring-arg
		       .arg(fcp.name())
		       .arg(scp.name())
		       .arg(fcp.wins())
		       .arg(scp.wins())
		       .arg(fcp.draws())
		       .arg(scoreRatio, 0, 'f', 3);
	}

	text += tournament->results();
	text += tr("\n%1 of %2 games finished.")
		.arg(tournament->finishedGameCount())
		.arg(tournament->finalGameCount());
	if (tournament->finishedGameCount() >= tournament->finalGameCount()
	|| tournament->isStopping() || tournament->isFinished())
		text.append("\n").append(tournament->outcomes());

	// Keep the scroll position when the text is updated
	auto vbar = m_resultsEdit->verticalScrollBar();
	auto hbar = m_resultsEdit->horizontalScrollBar();
	int vpos = vbar->value();
	int hpos = hbar->value();
	m_resultsEdit->setPlainText(text);
	vbar->setValue(vpos);
	hbar->setValue(hpos);
}
