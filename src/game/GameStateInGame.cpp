/****************************************************************************************
** Copyright (C) 2015 Simone Angeloni
** This file is part of The Little Invaders.
**
** The Little Invaders is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** The Little Invaders is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with The Little Invaders. If not, see <http://www.gnu.org/licenses/>
**
****************************************************************************************/

#include "GameStateInGame.h"
#include <ContainersUtils.h>
#include <IGraphicBitmap.h>
#include <IGraphicContainer.h>
#include <IGraphicTextfield.h>
#include <cassert>

#include "ISystemGlobalEnvironment.h"
extern utils::interfaces::SSystemGlobalEnvironment * gEnv;

namespace game {

	static const int DIFFICULY_MULT = 5;

	CGameStateInGame::CGameStateInGame()
		:m_pVariables(gEnv->pFramework->variablesManager())
		//VARIABLE VALUES - optimization to avoid retrieving the same value every cycle
		,VAR_ALIEN_ROWS_VALUE(m_pVariables->variable("g_AlienRows")->value<unsigned int>())
		,VAR_ALIEN_COLUMNS_VALUE(m_pVariables->variable("g_AlienColumns")->value<unsigned int>())
		,VAR_PLAYER_SPEED_VALUE(m_pVariables->variable("g_PlayerSpeedPxSec")->value<float>())
		,VAR_ALIEN_SPEED_VALUE(m_pVariables->variable("g_AlienSpeedPxSec")->value<float>())
		,VAR_ROCKET_SPEED_VALUE(m_pVariables->variable("g_RocketSpeedPxSec")->value<float>())
		,VAR_BOMB_SPEED_VALUE(m_pVariables->variable("g_BombSpeedPxSec")->value<float>())
		,VAR_BOMB_MAX_ON_SCREEN_VALUE(m_pVariables->variable("g_BombMaxOnScreen")->value<unsigned int>())
		,VAR_BOMB_PROBABILITY_VALUE(m_pVariables->variable("g_BombProbablityPercentage")->value<unsigned int>())
		,VAR_HEALTH_DAMAGE_VALUE(m_pVariables->variable("g_healthDamage")->value<unsigned int>())
		,VAR_KILL_SCORE_VALUE(m_pVariables->variable("g_killScore")->value<unsigned int>())
		,VAR_KILL_SCORE_SPECIAL_VALUE(m_pVariables->variable("g_killScoreSpecial")->value<unsigned int>())
	{
		m_pContainer = gEnv->pFramework->window()->addContainer();
		m_pContainer->setSize(gEnv->pFramework->window()->size());

		const int AreaHMult = m_pVariables->variable("g_GameAreaHMult")->value<unsigned int>();
		const int AreaVMult = m_pVariables->variable("g_GameAreaVMult")->value<unsigned int>();

		m_pGameArea = m_pContainer->addContainer();
		m_pGameArea->setSize(CGame::PICTURE_ALIEN_2.size().width() * AreaHMult, CGame::PICTURE_ALIEN_2.size().height() * AreaVMult);
		m_pGameArea->setPosition((m_pContainer->size().width() - m_pGameArea->size().width()) / 2, (m_pContainer->size().height() - m_pGameArea->size().height()) / 2);

		m_aliens.reserve(VAR_ALIEN_COLUMNS_VALUE * VAR_ALIEN_ROWS_VALUE);
		for (int column = 0; column < VAR_ALIEN_COLUMNS_VALUE; ++column)
		{
			for (int row = 0; row < VAR_ALIEN_ROWS_VALUE; ++row)
			{
				utils::interfaces::IGraphicBitmap * pAlien = m_pGameArea->addBitmap(CGame::PICTURE_ALIEN_2);
				m_aliens.push_back(pAlien);
				pAlien->setPosition(column * CGame::PICTURE_ALIEN_2.size().width(), row * CGame::PICTURE_ALIEN_2.size().height());
			}
		}

		m_pPlayer = m_pGameArea->addBitmap(CGame::PICTURE_PLAYER);
		m_pPlayer->setPosition(m_pGameArea->size().width() / 2, m_pGameArea->size().height() - CGame::PICTURE_PLAYER.size().height());

		m_pScoreTextField = m_pContainer->addTextfield();
		m_pScoreTextField->setPosition(m_pGameArea->position().x(), 19);

		m_pHealthTextField = m_pContainer->addTextfield();
		m_pHealthTextField->setPosition(m_pGameArea->position().x(), m_pContainer->size().height() - 35);
	}

	CGameStateInGame::~CGameStateInGame()
	{
		m_timer.removeListener(this);
		gEnv->pFramework->window()->removeItem(m_pContainer);
	}

	bool CGameStateInGame::init()
	{
		m_timer.addListener(this);
		m_timer.start();

		return true;
	}

	void CGameStateInGame::timeout()
	{
		checkCollisionsWithBorder();
		checkCollisionsWithPlayer();
		checkCollisionsWithRockets();

		moveAliens(m_timer.elapsed());
		moveRockets(m_timer.elapsed());
		moveBombs(m_timer.elapsed());

		spawnAliens();
		spawnBombs();

		updateScore();
		updateHealth();

		checkVictoryConditions();
	}

	void CGameStateInGame::onInput(utils::interfaces::CInputKey get_key, float deltaTime)
	{
		assert(m_pPlayer);
		assert(m_pGameArea);

		if (get_key.get_status() == utils::interfaces::CInputKey::key_status::inactive)
			return;

		switch (get_key.get_key())
		{
		case utils::interfaces::CInputKey::key::fire:
		{
			if (get_key.get_status() == utils::interfaces::CInputKey::key_status::press)
			{
				assert(m_pGameArea);
				utils::interfaces::IGraphicBitmap * pRocket = m_pGameArea->addBitmap(CGame::PICTURE_ROCKET);
				m_rockets.push_back(pRocket);
				utils::CPoint pos = m_pPlayer->position();
				pos.ry() -= m_pPlayer->size().height();
				pRocket->setPosition(pos);
			}
		}
		break;

		case utils::interfaces::CInputKey::key::left:
		{
			utils::CPoint pos = m_pPlayer->position();
			pos.rx() -= (deltaTime * VAR_PLAYER_SPEED_VALUE);
			pos.rx() = std::max(0., pos.x());
			m_pPlayer->setPosition(pos);
		}
		break;

		case utils::interfaces::CInputKey::key::right:
		{
			utils::CPoint pos = m_pPlayer->position();
			pos.rx() += (deltaTime * VAR_PLAYER_SPEED_VALUE);
			pos.rx() = std::min(pos.x(), m_pGameArea->size().width() - CGame::PICTURE_PLAYER.size().width());
			m_pPlayer->setPosition(pos);
		}
		break;
		}
	}

	void CGameStateInGame::checkCollisionsWithBorder()
	{
		assert(m_pGameArea);
		m_aliensMoveDown = false;

		utils::interfaces::IGraphicItem::TGraphicItems collidingitems = m_pGameArea->collidingItems(m_pGameArea, utils::interfaces::IGraphicItem::collision_mode::intersect_not_contain);

		auto it_end = collidingitems.end();
		for (auto it = collidingitems.begin(); it != it_end; ++it)
		{
			utils::interfaces::IGraphicItem * pItem = (*it);

			if (isAlien(pItem))
			{
				m_aliensMoveLeft = !m_aliensMoveLeft;
				m_aliensMoveDown = true;
				return; // One alien reaching the borders is enough to say that aliens have to move down
			}
			else if (isSuperAlien(pItem))
			{
				delete m_pSuperAlien;
				m_pSuperAlien = nullptr;
			}
			else if (isRocket(pItem))
			{
				utils::containers::gFindAndErase(m_rockets, pItem);
				delete pItem;
			}
			else if (isBomb(pItem))
			{
				utils::containers::gFindAndErase(m_bombs, pItem);
				m_pContainer->removeItem(pItem);
			}
		}
	}

	void CGameStateInGame::checkCollisionsWithPlayer()
	{
		assert(m_pGameArea && m_pPlayer);

		utils::interfaces::IGraphicItem::TGraphicItems collidingitems = m_pGameArea->collidingItems(m_pPlayer, utils::interfaces::IGraphicItem::collision_mode::intersect);

		auto it_end = collidingitems.end();
		for (auto it = collidingitems.begin(); it != it_end; ++it)
		{
			utils::interfaces::IGraphicItem * pItem = (*it);

			if (isAlien(pItem))
			{
				// Aliens vector simulates a bydimentional array of type aliens[ROWS][COLUMNS] so the size is kept unchanged to retrieves aliens position at wish
				utils::containers::gFindAndReplace(m_aliens, pItem, (utils::interfaces::IGraphicItem *)nullptr);
				delete pItem;
			}
			else if (isBomb(pItem))
			{
				utils::containers::gFindAndErase(m_bombs, pItem);
				delete pItem;
			}

			gEnv->pGame->onEvent(utils::interfaces::SGameEvent(CGame::gameevent_health, VAR_HEALTH_DAMAGE_VALUE));
		}
	}

	void CGameStateInGame::checkCollisionsWithRockets()
	{
		assert(m_pGameArea);

		auto itRocket_end = m_rockets.end();
		for (auto itRocket = m_rockets.begin(); itRocket != itRocket_end; ++itRocket)
		{
			utils::interfaces::IGraphicItem * pRocket = (*itRocket);
			utils::interfaces::IGraphicItem::TGraphicItems collidingitems = m_pGameArea->collidingItems(pRocket, utils::interfaces::IGraphicItem::collision_mode::intersect);

			if (collidingitems.empty())
				continue;

			// Forcing rocket to collide with maximum 1 alien :))
			utils::interfaces::IGraphicItem * pItem = collidingitems.at(0);

			if (isAlien(pItem))
			{
				utils::containers::gFindAndReplace(m_aliens, pItem, (utils::interfaces::IGraphicItem *)nullptr);
				delete pItem;

				(*itRocket) = nullptr;
				delete pRocket;

				gEnv->pGame->onEvent(utils::interfaces::SGameEvent(CGame::gameevent_score, VAR_KILL_SCORE_VALUE));
			}
			else if (isSuperAlien(pItem))
			{
				delete m_pSuperAlien;
				m_pSuperAlien = nullptr;

				(*itRocket) = nullptr;
				delete pRocket;

				gEnv->pGame->onEvent(utils::interfaces::SGameEvent(CGame::gameevent_score, VAR_KILL_SCORE_SPECIAL_VALUE));
			}
		}

		utils::containers::gFindAndEraseAll(m_rockets, (utils::interfaces::IGraphicItem *)nullptr);
	}

	void CGameStateInGame::checkVictoryConditions()
	{
		if (aliveAliens().empty())
			gEnv->pGame->onEvent(utils::interfaces::SGameEvent(CGame::gameevent_exit, 1));

		if (static_cast<CGame*>(gEnv->pGame)->lifes() <= 0)
			gEnv->pGame->onEvent(utils::interfaces::SGameEvent(CGame::gameevent_exit, 0));

		if (isAnyAlienBypassed())
			gEnv->pGame->onEvent(utils::interfaces::SGameEvent(CGame::gameevent_exit, 0));
	}

	void CGameStateInGame::moveAliens(float deltaTime)
	{
		const float move = deltaTime * (VAR_ALIEN_SPEED_VALUE + m_difficulty);

		auto it_end = m_aliens.end();
		for (auto it = m_aliens.begin(); it != it_end; ++it)
		{
			utils::interfaces::IGraphicItem * pAlien = (*it);
			if (!pAlien)
				continue;

			utils::CPoint pos = pAlien->position();
			if (m_aliensMoveDown)
			{
				pos.ry() += CGame::PICTURE_ALIEN_2.size().height();
				pos.rx() += m_aliensMoveLeft ? -1 : 1; // Makes sure aliens do not collide with borders on next frame
			}
			else
			{
				pos.rx() += m_aliensMoveLeft ? -move : move;
			}
			pAlien->setPosition(pos);
		}

		if (m_aliensMoveDown)
		{
			m_difficulty += DIFFICULY_MULT;
			m_aliensMoveDown = false;
		}

		if (m_pSuperAlien)
		{
			utils::CPoint pos = m_pSuperAlien->position();
			pos.rx() += move;
			m_pSuperAlien->setPosition(pos);
		}
	}

	void CGameStateInGame::moveRockets(float deltaTime)
	{
		const float move = deltaTime * VAR_ROCKET_SPEED_VALUE;

		auto it_end = m_rockets.end();
		for (auto it = m_rockets.begin(); it != it_end; ++it)
		{
			utils::interfaces::IGraphicItem * pRocket = (*it);

			utils::CPoint pos = pRocket->position();
			pos.ry() -= move;
			pRocket->setPosition(pos);
		}
	}

	void CGameStateInGame::moveBombs(float deltaTime)
	{
		const float move = deltaTime * VAR_BOMB_SPEED_VALUE;

		auto it_end = m_bombs.end();
		for (auto it = m_bombs.begin(); it != it_end; ++it)
		{
			utils::interfaces::IGraphicItem * pBomb = (*it);

			utils::CPoint pos = pBomb->position();
			pos.ry() += move;
			pBomb->setPosition(pos);
		}
	}

	void CGameStateInGame::spawnBombs()
	{
		assert(m_pGameArea);

		if (m_bombs.size() >= VAR_BOMB_MAX_ON_SCREEN_VALUE)
			return;

		utils::interfaces::IGraphicItem::TGraphicItems aliens = freeAliens();
		const size_t alienSize = aliens.size();

		if (aliens.empty())
			return;

		int randomAlien = gEnv->pFramework->random(alienSize);
		utils::interfaces::IGraphicItem * pAlien = aliens.at(randomAlien);

		if (gEnv->pFramework->random(500) >= VAR_BOMB_PROBABILITY_VALUE * 5)
			return;

		utils::interfaces::IGraphicBitmap * pBomb = m_pGameArea->addBitmap(CGame::PICTURE_BOMB);
		m_bombs.push_back(pBomb);

		utils::CPoint pos = pAlien->position();
		pos.ry() += pAlien->size().height();
		pBomb->setPosition(pos);
	}

	void CGameStateInGame::spawnAliens()
	{
		// For now takes care to spawn superAlien only

		assert(m_pGameArea);

		if (m_pSuperAlien)
			return;

		if (gEnv->pFramework->random(2500) != 1)
			return;

		// Verifies if there is any alien in the top row where the superAlien is spawned
		if (!m_pGameArea->collidingItems(utils::CRectangle(0, 0, m_pGameArea->size().width(), CGame::PICTURE_ALIEN_2.size().height()), utils::interfaces::IGraphicItem::collision_mode::intersect).empty())
			return;

		m_pSuperAlien = m_pGameArea->addBitmap(CGame::PICTURE_ALIEN_1);
	}

	void CGameStateInGame::updateScore()
	{
		assert(m_pScoreTextField);
		m_pScoreTextField->setText("SCORE: %d", static_cast<CGame*>(gEnv->pGame)->score());
	}

	void CGameStateInGame::updateHealth()
	{
		assert(m_pHealthTextField);
		m_pHealthTextField->setText("HEALTH: %d", static_cast<CGame*>(gEnv->pGame)->lifes());
	}

	bool CGameStateInGame::isSuperAlien(utils::interfaces::IGraphicItem * pItem) const
	{
		return pItem == m_pSuperAlien;
	}

	bool CGameStateInGame::isPlayer(utils::interfaces::IGraphicItem * pItem) const
	{
		return pItem == m_pPlayer;
	}

	utils::interfaces::IGraphicItem::TGraphicItems CGameStateInGame::aliveAliens() const
	{
		utils::interfaces::IGraphicItem::TGraphicItems aliens;

		auto it_end = m_aliens.end();
		for (auto it = m_aliens.begin(); it != it_end; ++it)
		{
			utils::interfaces::IGraphicItem * pAlien = (*it);
			if (pAlien)
			{
				aliens.push_back(pAlien);
			}
		}

		return aliens;
	}

	bool CGameStateInGame::isAnyAlienBypassed() const
	{
		auto it_end = m_aliens.end();
		for (auto it = m_aliens.begin(); it != it_end; ++it)
		{
			utils::interfaces::IGraphicItem * pAlien = (*it);
			if (pAlien && pAlien->position().y() > m_pPlayer->position().y())
			{
				return true;
			}
		}

		return false;
	}

	utils::interfaces::IGraphicItem::TGraphicItems CGameStateInGame::freeAliens() const
	{
		utils::interfaces::IGraphicItem::TGraphicItems aliens;

		// From "bottom" to "top" retrieves the aliens able to shoot
		for (int column = 0; column < VAR_ALIEN_COLUMNS_VALUE; ++column)
		{
			utils::interfaces::IGraphicItem * pAlien = nullptr;

			for (int row = VAR_ALIEN_ROWS_VALUE - 1; row >= 0; --row)
			{
				pAlien = m_aliens.at(row + (column * VAR_ALIEN_ROWS_VALUE));
				if (pAlien)
					break;
			}

			if (pAlien)
			{
				aliens.push_back(pAlien);
				continue;
			}
		}

		return aliens;
	}

} // namespace game
