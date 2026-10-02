/*
 * RemoteRecruitmentTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "../mock/mock_IGameInfoCallback.h"

#include "../../lib/GameSettings.h"
#include "../../lib/json/JsonNode.h"
#include "../../lib/mapObjects/CGDwelling.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CGTownInstance.h"

namespace test
{
using namespace ::testing;

class RemoteRecruitmentCallbackMock : public IGameInfoCallbackMock
{
public:
	std::unique_ptr<GameSettings> settings;

	const IGameSettings & getSettings() const override
	{
		return *settings;
	}
};

/// Covers validation used by CGameHandler::recruitCreatures for recruiting without a visiting hero
class RemoteRecruitmentTest : public Test
{
public:
	const PlayerColor player = PlayerColor(0);
	const PlayerColor enemy = PlayerColor(1);
	const CreatureID creature = CreatureID::ARCHER;
	const CreatureID warMachine = CreatureID::BALLISTA;

	NiceMock<RemoteRecruitmentCallbackMock> cb;
	std::shared_ptr<CGDwelling> dwelling;
	std::shared_ptr<CGTownInstance> town;
	std::shared_ptr<CGHeroInstance> hero;

	void SetUp() override
	{
		setRemoteRecruitment(true);

		dwelling = std::make_shared<CGDwelling>(&cb);
		dwelling->ID = Obj::CREATURE_GENERATOR1;
		dwelling->id = ObjectInstanceID(1);
		dwelling->tempOwner = player;

		town = std::make_shared<CGTownInstance>(&cb);
		town->id = ObjectInstanceID(2);
		town->tempOwner = player;

		hero = std::make_shared<CGHeroInstance>(&cb);
		hero->id = ObjectInstanceID(3);
		hero->tempOwner = player;

		ON_CALL(cb, getTown(town->id)).WillByDefault(Return(town.get()));
	}

	void setRemoteRecruitment(bool allowed)
	{
		cb.settings = std::make_unique<GameSettings>();
		cb.settings->addOverride(EGameSettings::DWELLINGS_ALLOW_REMOTE_RECRUITMENT, JsonNode(allowed));
	}
};

TEST_F(RemoteRecruitmentTest, ownedDwellingToOwnedTownIsAccepted)
{
	EXPECT_TRUE(dwelling->canRecruitRemotely(player, town.get(), creature));
}

TEST_F(RemoteRecruitmentTest, ownedDwellingToGarrisonedHeroIsAccepted)
{
	hero->setVisitedTown(town.get(), true);
	EXPECT_TRUE(dwelling->canRecruitRemotely(player, hero.get(), creature));
}

TEST_F(RemoteRecruitmentTest, unownedDwellingIsRejected)
{
	dwelling->tempOwner = PlayerColor::NEUTRAL;
	EXPECT_FALSE(dwelling->canRecruitRemotely(player, town.get(), creature));

	dwelling->tempOwner = enemy;
	EXPECT_FALSE(dwelling->canRecruitRemotely(player, town.get(), creature));
}

TEST_F(RemoteRecruitmentTest, unflaggableDwellingIsRejected)
{
	dwelling->tempOwner = PlayerColor::UNFLAGGABLE;
	EXPECT_FALSE(dwelling->canRecruitRemotely(player, town.get(), creature));
}

TEST_F(RemoteRecruitmentTest, otherPlayerTownIsRejected)
{
	town->tempOwner = enemy;
	EXPECT_FALSE(dwelling->canRecruitRemotely(player, town.get(), creature));

	hero->tempOwner = enemy;
	hero->setVisitedTown(town.get(), true);
	EXPECT_FALSE(dwelling->canRecruitRemotely(player, hero.get(), creature));
}

TEST_F(RemoteRecruitmentTest, warMachineIsRejected)
{
	EXPECT_FALSE(dwelling->canRecruitRemotely(player, town.get(), warMachine));
}

TEST_F(RemoteRecruitmentTest, disabledSettingIsRejected)
{
	setRemoteRecruitment(false);
	EXPECT_FALSE(dwelling->canRecruitRemotely(player, town.get(), creature));

	hero->setVisitedTown(town.get(), true);
	EXPECT_FALSE(dwelling->canRecruitRemotely(player, hero.get(), creature));
}

TEST_F(RemoteRecruitmentTest, heroNotGarrisonedIsRejected)
{
	EXPECT_FALSE(dwelling->canRecruitRemotely(player, hero.get(), creature));

	hero->setVisitedTown(town.get(), false);
	EXPECT_FALSE(dwelling->canRecruitRemotely(player, hero.get(), creature));
}

TEST_F(RemoteRecruitmentTest, townAsSourceIsRejected)
{
	auto otherTown = std::make_shared<CGTownInstance>(&cb);
	otherTown->id = ObjectInstanceID(4);
	otherTown->tempOwner = player;
	EXPECT_FALSE(otherTown->canRecruitRemotely(player, town.get(), creature));
}

}
