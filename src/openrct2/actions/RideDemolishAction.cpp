/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "RideDemolishAction.h"

#include "../Cheats.h"
#include "../Context.h"
#include "../GameState.h"
#include "../core/MemoryStream.h"
#include "../drawing/Drawing.h"
#include "../interface/Window.h"
#include "../localisation/Localisation.h"
#include "../management/NewsItem.h"
#include "../ride/Ride.h"
#include "../ride/RideData.h"
#include "../ui/UiContext.h"
#include "../ui/WindowManager.h"
#include "../world/Banner.h"
#include "../world/EntityList.h"
#include "../world/Park.h"
#include "../world/Sprite.h"
#include "MazeSetTrackAction.h"
#include "TrackRemoveAction.h"

using namespace OpenRCT2;

RideDemolishAction::RideDemolishAction(ride_id_t rideIndex, uint8_t modifyType)
    : _rideIndex(rideIndex)
    , _modifyType(modifyType)
{
}

void RideDemolishAction::AcceptParameters(GameActionParameterVisitor& visitor)
{
    visitor.Visit("ride", _rideIndex);
    visitor.Visit("modifyType", _modifyType);
}

uint32_t RideDemolishAction::GetCooldownTime() const
{
    return 1000;
}

void RideDemolishAction::Serialise(DataSerialiser& stream)
{
    GameAction::Serialise(stream);

    stream << DS_TAG(_rideIndex) << DS_TAG(_modifyType);
}

GameActions::Result::Ptr RideDemolishAction::Query() const
{
    auto ride = get_ride(_rideIndex);
    if (ride == nullptr)
    {
        log_warning("Invalid game command for ride %u", uint32_t(_rideIndex));
        return std::make_unique<GameActions::Result>(GameActions::Status::InvalidParameters, STR_CANT_DEMOLISH_RIDE, STR_NONE);
    }

    if (ride->lifecycle_flags & (RIDE_LIFECYCLE_INDESTRUCTIBLE | RIDE_LIFECYCLE_INDESTRUCTIBLE_TRACK)
        && _modifyType == RIDE_MODIFY_DEMOLISH)
    {
        return std::make_unique<GameActions::Result>(
            GameActions::Status::NoClearance, STR_CANT_DEMOLISH_RIDE,
            STR_LOCAL_AUTHORITY_FORBIDS_DEMOLITION_OR_MODIFICATIONS_TO_THIS_RIDE);
    }

    GameActions::Result::Ptr result = std::make_unique<GameActions::Result>();

    if (_modifyType == RIDE_MODIFY_RENEW)
    {
        if (ride->status != RIDE_STATUS_CLOSED && ride->status != RIDE_STATUS_SIMULATING)
        {
            return std::make_unique<GameActions::Result>(
                GameActions::Status::Disallowed, STR_CANT_REFURBISH_RIDE, STR_MUST_BE_CLOSED_FIRST);
        }

        if (ride->num_riders > 0)
        {
            return std::make_unique<GameActions::Result>(
                GameActions::Status::Disallowed, STR_CANT_REFURBISH_RIDE, STR_RIDE_NOT_YET_EMPTY);
        }

        if (!(ride->lifecycle_flags & RIDE_LIFECYCLE_EVER_BEEN_OPENED)
            || ride->GetRideTypeDescriptor().AvailableBreakdowns == 0)
        {
            return std::make_unique<GameActions::Result>(
                GameActions::Status::Disallowed, STR_CANT_REFURBISH_RIDE, STR_CANT_REFURBISH_NOT_NEEDED);
        }

        result->ErrorTitle = STR_CANT_REFURBISH_RIDE;
        result->Cost = GetRefurbishPrice(ride);
    }

    return result;
}

GameActions::Result::Ptr RideDemolishAction::Execute() const
{
    auto ride = get_ride(_rideIndex);
    if (ride == nullptr)
    {
        log_warning("Invalid game command for ride %u", uint32_t(_rideIndex));
        return std::make_unique<GameActions::Result>(GameActions::Status::InvalidParameters, STR_CANT_DEMOLISH_RIDE, STR_NONE);
    }

    switch (_modifyType)
    {
        case RIDE_MODIFY_DEMOLISH:
            return DemolishRide(ride);
        case RIDE_MODIFY_RENEW:
            return RefurbishRide(ride);
    }

    return std::make_unique<GameActions::Result>(GameActions::Status::InvalidParameters, STR_CANT_DO_THIS);
}

GameActions::Result::Ptr RideDemolishAction::DemolishRide(Ride* ride) const
{
    money32 refundPrice = DemolishTracks();

    ride_clear_for_construction(ride);
    ride_remove_peeps(ride);
    ride->StopGuestsQueuing();

    sub_6CB945(ride);
    ride_clear_leftover_entrances(ride);
    News::DisableNewsItems(News::ItemType::Ride, _rideIndex);

    for (BannerIndex i = 0; i < MAX_BANNERS; i++)
    {
        auto banner = GetBanner(i);
        if (!banner->IsNull() && banner->flags & BANNER_FLAG_LINKED_TO_RIDE && banner->ride_index == _rideIndex)
        {
            banner->flags &= ~BANNER_FLAG_LINKED_TO_RIDE;
            banner->text = {};
        }
    }

    for (auto peep : EntityList<Guest>())
    {
        uint8_t ride_id_bit = _rideIndex % 8;
        uint8_t ride_id_offset = _rideIndex / 8;

        // clear ride from potentially being in RidesBeenOn
        peep->RidesBeenOn[ride_id_offset] &= ~(1 << ride_id_bit);
        if (peep->State == PeepState::Watching)
        {
            if (peep->CurrentRide == _rideIndex)
            {
                peep->CurrentRide = RIDE_ID_NULL;
                if (peep->TimeToStand >= 50)
                {
                    // make peep stop watching the ride
                    peep->TimeToStand = 50;
                }
            }
        }

        // remove any free voucher for this ride from peep
        if (peep->HasItem(ShopItem::Voucher))
        {
            if (peep->VoucherType == VOUCHER_TYPE_RIDE_FREE && peep->VoucherRideId == _rideIndex)
            {
                peep->RemoveItem(ShopItem::Voucher);
            }
        }

        // remove any photos of this ride from peep
        if (peep->HasItem(ShopItem::Photo))
        {
            if (peep->Photo1RideRef == _rideIndex)
            {
                peep->RemoveItem(ShopItem::Photo);
            }
        }
        if (peep->HasItem(ShopItem::Photo2))
        {
            if (peep->Photo2RideRef == _rideIndex)
            {
                peep->RemoveItem(ShopItem::Photo2);
            }
        }
        if (peep->HasItem(ShopItem::Photo3))
        {
            if (peep->Photo3RideRef == _rideIndex)
            {
                peep->RemoveItem(ShopItem::Photo3);
            }
        }
        if (peep->HasItem(ShopItem::Photo4))
        {
            if (peep->Photo4RideRef == _rideIndex)
            {
                peep->RemoveItem(ShopItem::Photo4);
            }
        }

        if (peep->GuestHeadingToRideId == _rideIndex)
        {
            peep->GuestHeadingToRideId = RIDE_ID_NULL;
        }
        if (peep->FavouriteRide == _rideIndex)
        {
            peep->FavouriteRide = RIDE_ID_NULL;
        }

        for (int32_t i = 0; i < PEEP_MAX_THOUGHTS; i++)
        {
            // Don't touch items after the first NONE thought as they are not valid
            // fixes issues with clearing out bad thought data in multiplayer
            if (peep->Thoughts[i].type == PeepThoughtType::None)
                break;

            if (peep->Thoughts[i].type != PeepThoughtType::None && peep->Thoughts[i].item == _rideIndex)
            {
                // Clear top thought, push others up
                memmove(&peep->Thoughts[i], &peep->Thoughts[i + 1], sizeof(rct_peep_thought) * (PEEP_MAX_THOUGHTS - i - 1));
                peep->Thoughts[PEEP_MAX_THOUGHTS - 1].type = PeepThoughtType::None;
                peep->Thoughts[PEEP_MAX_THOUGHTS - 1].item = PEEP_THOUGHT_ITEM_NONE;
                // Next iteration, check the new thought at this index
                i--;
            }
        }
    }

    MarketingCancelCampaignsForRide(_rideIndex);

    auto res = std::make_unique<GameActions::Result>();
    res->Expenditure = ExpenditureType::RideConstruction;
    res->Cost = refundPrice;

    if (!ride->overall_view.isNull())
    {
        auto xy = ride->overall_view.ToTileCentre();
        res->Position = { xy, tile_element_height(xy) };
    }

    ride->Delete();
    gParkValue = GetContext()->GetGameState()->GetPark().CalculateParkValue();

    // Close windows related to the demolished ride
    if (!(GetFlags() & GAME_COMMAND_FLAG_ALLOW_DURING_PAUSED))
    {
        window_close_by_number(WC_RIDE_CONSTRUCTION, _rideIndex);
    }
    window_close_by_number(WC_RIDE, _rideIndex);
    window_close_by_number(WC_DEMOLISH_RIDE_PROMPT, _rideIndex);
    window_close_by_class(WC_NEW_CAMPAIGN);

    // Refresh windows that display the ride name
    auto windowManager = OpenRCT2::GetContext()->GetUiContext()->GetWindowManager();
    windowManager->BroadcastIntent(Intent(INTENT_ACTION_REFRESH_CAMPAIGN_RIDE_LIST));
    windowManager->BroadcastIntent(Intent(INTENT_ACTION_REFRESH_RIDE_LIST));
    windowManager->BroadcastIntent(Intent(INTENT_ACTION_REFRESH_GUEST_LIST));

    scrolling_text_invalidate();
    gfx_invalidate_screen();

    return res;
}

money32 RideDemolishAction::MazeRemoveTrack(const CoordsXYZD& coords) const
{
    auto setMazeTrack = MazeSetTrackAction(coords, false, _rideIndex, GC_SET_MAZE_TRACK_FILL);
    setMazeTrack.SetFlags(GetFlags());

    auto execRes = GameActions::ExecuteNested(&setMazeTrack);
    if (execRes->Error == GameActions::Status::Ok)
    {
        return execRes->Cost;
    }

    return MONEY32_UNDEFINED;
}

money32 RideDemolishAction::DemolishTracks() const
{
    money32 refundPrice = 0;

    uint8_t oldpaused = gGamePaused;
    gGamePaused = 0;

    tile_element_iterator it;

    tile_element_iterator_begin(&it);
    while (tile_element_iterator_next(&it))
    {
        if (it.element->GetType() != TILE_ELEMENT_TYPE_TRACK)
            continue;

        if (it.element->AsTrack()->GetRideIndex() != static_cast<ride_id_t>(_rideIndex))
            continue;

        auto location = CoordsXYZD(TileCoordsXY(it.x, it.y).ToCoordsXY(), it.element->GetBaseZ(), it.element->GetDirection());
        auto type = it.element->AsTrack()->GetTrackType();

        if (type != TrackElemType::Maze)
        {
            auto trackRemoveAction = TrackRemoveAction(type, it.element->AsTrack()->GetSequenceIndex(), location);
            trackRemoveAction.SetFlags(GAME_COMMAND_FLAG_NO_SPEND);

            auto removRes = GameActions::ExecuteNested(&trackRemoveAction);

            if (removRes->Error != GameActions::Status::Ok)
            {
                tile_element_remove(it.element);
            }
            else
            {
                refundPrice += removRes->Cost;
            }

            tile_element_iterator_restart_for_tile(&it);
            continue;
        }

        static constexpr const CoordsXY DirOffsets[] = {
            { 0, 0 },
            { 0, 16 },
            { 16, 16 },
            { 16, 0 },
        };

        for (Direction dir : ALL_DIRECTIONS)
        {
            const CoordsXYZ off = { DirOffsets[dir], 0 };
            money32 removePrice = MazeRemoveTrack({ location + off, dir });
            if (removePrice != MONEY32_UNDEFINED)
                refundPrice += removePrice;
            else
                break;
        }

        tile_element_iterator_restart_for_tile(&it);
    }

    gGamePaused = oldpaused;
    return refundPrice;
}

GameActions::Result::Ptr RideDemolishAction::RefurbishRide(Ride* ride) const
{
    auto res = std::make_unique<GameActions::Result>();
    res->Expenditure = ExpenditureType::RideConstruction;
    res->Cost = GetRefurbishPrice(ride);

    ride->Renew();

    ride->lifecycle_flags &= ~RIDE_LIFECYCLE_EVER_BEEN_OPENED;
    ride->last_crash_type = RIDE_CRASH_TYPE_NONE;

    ride->window_invalidate_flags |= RIDE_INVALIDATE_RIDE_MAINTENANCE | RIDE_INVALIDATE_RIDE_CUSTOMER;

    if (!ride->overall_view.isNull())
    {
        auto location = ride->overall_view.ToTileCentre();
        res->Position = { location, tile_element_height(location) };
    }

    window_close_by_number(WC_DEMOLISH_RIDE_PROMPT, _rideIndex);

    return res;
}

money32 RideDemolishAction::GetRefurbishPrice(const Ride* ride) const
{
    return -GetRefundPrice(ride) / 2;
}

money32 RideDemolishAction::GetRefundPrice(const Ride* ride) const
{
    return ride_get_refund_price(ride);
}
