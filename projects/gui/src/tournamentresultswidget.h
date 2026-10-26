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

#ifndef TOURNAMENTRESULTSWIDGET_H
#define TOURNAMENTRESULTSWIDGET_H

#include <QWidget>
#include <QPointer>

class QPlainTextEdit;
class Tournament;

/*!
 * \brief A widget that shows the standings of a tournament.
 *
 * Used by the tournament results dialog and the tournament
 * standings dock of the main window.
 */
class TournamentResultsWidget : public QWidget
{
	Q_OBJECT

	public:
		/*! Creates a new TournamentResultsWidget. */
		explicit TournamentResultsWidget(QWidget* parent = nullptr);

		/*!
		 * Follows \a tournament: the standings are updated
		 * automatically whenever a game finishes and when the
		 * tournament ends.
		 */
		void setTournament(Tournament* tournament);
		/*! Shows the current standings of \a tournament. */
		void updateResults(Tournament* tournament);

	private slots:
		void onTournamentChanged();

	private:
		QPlainTextEdit* m_resultsEdit;
		QPointer<Tournament> m_tournament;
};

#endif // TOURNAMENTRESULTSWIDGET_H
