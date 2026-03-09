#include "SteamStuff.h"
#include "RemotePlayInviteHandler.h"

RemotePlayInviteHandler::RemotePlayInviteHandler() :
    m_enabledDesktopStreaming(false),
    m_remoteGuestID(1),
    m_nonsteamAppID(0),
    m_groupID(1),
    m_remoteInviteResultCb(this, &RemotePlayInviteHandler::OnRemotePlayInviteResult),
    m_remoteStopCb(this, &RemotePlayInviteHandler::OnRemotePlayStop)
{
}

AppId_t RemotePlayInviteHandler::GetNonSteamAppID()
{
    return m_nonsteamAppID;
}

void RemotePlayInviteHandler::SendInvite(CSteamID invitee)
{
    CGameID gameID = GetRunningGameID();

    if (!gameID.IsValid())
    {
        return;
    }

    RemotePlayPlayer_t rppInvitee = { invitee, m_remoteGuestID, m_groupID, 0, 0, 0 };
    ++m_remoteGuestID;

    // some games (especially non-steam titles and emulators) do not automatically
    // forward guest controller input via the normal "game" streaming path.
    // enabling desktop streaming before the invite ensures the guest will always
    // be able to send keyboard/mouse/controller events to the host.  We also
    // explicitly allow controller input for the specific player after creating
    // the session.
    
    // make sure desktop streaming is enabled whenever we are about to send an
    // invite; this is a no-op for most Steam games but is required for desktop
    // capture scenarios.
    GClientContext()->RemoteClientManager()->SetStreamingDesktopToRemotePlayTogetherEnabled(true);
    m_enabledDesktopStreaming = true;

    if (gameID.IsSteamApp() && gameID.AppID() != m_nonsteamAppID)
    {
        GClientContext()->RemoteClientManager()->BCreateRemotePlayInviteAndSession(rppInvitee, gameID.AppID());
    }
    else
    {
        GClientContext()->RemoteClientManager()->BCreateRemotePlayInviteAndSession(rppInvitee, m_nonsteamAppID);
    }

    // explicitly give the invited player permission to send controller, keyboard
    // and mouse input.  this addresses situations where guests see the host but
    // their controllers (or other input) are ignored.
    GClientContext()->RemoteClientManager()->SetPerUserControllerInputEnabled(rppInvitee, true);
    GClientContext()->RemoteClientManager()->SetPerUserKeyboardInputEnabled(rppInvitee, true);
    GClientContext()->RemoteClientManager()->SetPerUserMouseInputEnabled(rppInvitee, true);
}

void RemotePlayInviteHandler::CancelInvite(CSteamID invitee, uint32 guestID)
{
    if(GClientContext()->RemoteClientManager()->BIsStreamingSessionActive())
    {
        RemotePlayPlayer_t rppInvitee = { invitee, guestID, m_groupID, 0, 0, 0 };
        GClientContext()->RemoteClientManager()->CancelRemotePlayInviteAndSession(rppInvitee);
    }
}

void RemotePlayInviteHandler::SetNonSteamAppID(AppId_t appID)
{
    m_nonsteamAppID = appID;
}

void RemotePlayInviteHandler::SetGuestID(uint32 guestID)
{
    m_remoteGuestID = guestID;
}

void RemotePlayInviteHandler::OnRemotePlayInviteResult(RemotePlayInviteResult_t* inviteResultCb)
{
    if (inviteResultCb->m_eResult == k_ERemoteClientLaunchResultOK)
    {
        // when the session is successfully created we also proactively enable
        // all input types for the joining player.  doing this here ensures the
        // setting is applied after Steam has finished the launch handshake.
        RemotePlayPlayer_t player = inviteResultCb->m_player;
        GClientContext()->RemoteClientManager()->SetPerUserControllerInputEnabled(player, true);
        GClientContext()->RemoteClientManager()->SetPerUserKeyboardInputEnabled(player, true);
        GClientContext()->RemoteClientManager()->SetPerUserMouseInputEnabled(player, true);

        if (player.m_playerID.IsValid())
        {
            char* buf = new char[1280];
            sprintf(buf, "Follow this link to join remote game: %s", inviteResultCb->m_szConnectURL);
            GClientContext()->SteamFriends()->ReplyToFriendMessage(player.m_playerID, buf);
            delete[] buf;
        }
        GClientContext()->RemoteClientManager()->ShowRemotePlayTogetherUI(GetRunningGameID().AppID());
    }
}

void RemotePlayInviteHandler::OnRemotePlayStop(RemoteClientStopStreamSession_t* streamStopCb)
{
    if (streamStopCb->m_player.m_playerID == GClientContext()->SteamUser()->GetSteamID())
    {
        GClientContext()->RemoteClientManager()->SetStreamingDesktopToRemotePlayTogetherEnabled(false);
        m_remoteGuestID = 1;
    }
}
