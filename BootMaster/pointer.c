/*
 * BootMaster/pointer.c
 * Pointer device functions
 *
 * Copyright (c) 2018 CJ Vaughter
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "pointer.h"
#include "global.h"
#include "screenmgt.h"
#include "icns.h"
#include "../include/refit_call_wrapper.h"

extern VOID MyFreePool (IN OUT VOID *Pointer);

UINTN                           NumAPointerDevices = 0;
UINTN                           NumSPointerDevices = 0;
EFI_HANDLE                     *APointerHandles    = NULL;
EFI_HANDLE                     *SPointerHandles    = NULL;
EFI_GUID                        APointerGuid       = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;
EFI_GUID                        SPointerGuid       = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
EFI_ABSOLUTE_POINTER_PROTOCOL **APointerProtocol   = NULL;
EFI_SIMPLE_POINTER_PROTOCOL   **SPointerProtocol   = NULL;

BOOLEAN PointerAvailable = FALSE;
BOOLEAN gSuppressPointerDraw = TRUE;
UINTN LastXPos = 0, LastYPos = 0;
EG_IMAGE* MouseImage = NULL;
EG_IMAGE* Background = NULL;

POINTER_STATE State;

////////////////////////////////////////////////////////////////////////////////
// Initialize all pointer devices
////////////////////////////////////////////////////////////////////////////////
VOID pdInitialize() {
    pdCleanup();

    if (!GlobalConfig.EnableMouse && !GlobalConfig.EnableTouch) {
        return;
    }

    UINTN NumPointerHandles = 0;
    EFI_STATUS handlestatus = REFIT_CALL_5_WRAPPER(gBS->LocateHandleBuffer, ByProtocol, &APointerGuid, NULL, &NumPointerHandles, &APointerHandles);
    if (!EFI_ERROR(handlestatus)) {
        APointerProtocol = AllocatePool(sizeof(EFI_ABSOLUTE_POINTER_PROTOCOL*) * NumPointerHandles);
        UINTN Index;
        for(Index = 0; Index < NumPointerHandles; Index++) {
            EFI_STATUS status = REFIT_CALL_6_WRAPPER(gBS->OpenProtocol, APointerHandles[Index], &APointerGuid, (VOID **) &APointerProtocol[NumAPointerDevices], SelfImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
            if (status == EFI_SUCCESS) {
                NumAPointerDevices++;
                REFIT_CALL_1_WRAPPER(gBS->Stall, 5 * 1000);
            }
        }
    } else {
        GlobalConfig.EnableTouch = FALSE;
    }

    NumPointerHandles = 0;
    handlestatus = REFIT_CALL_5_WRAPPER(gBS->LocateHandleBuffer, ByProtocol, &SPointerGuid, NULL, &NumPointerHandles, &SPointerHandles);
    if(!EFI_ERROR(handlestatus)) {
        SPointerProtocol = AllocatePool(sizeof(EFI_SIMPLE_POINTER_PROTOCOL*) * NumPointerHandles);
        UINTN Index;
        for(Index = 0; Index < NumPointerHandles; Index++) {
            EFI_STATUS status = REFIT_CALL_6_WRAPPER(gBS->OpenProtocol, SPointerHandles[Index], &SPointerGuid, (VOID **) &SPointerProtocol[NumSPointerDevices], SelfImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
            if (status == EFI_SUCCESS) {
                NumSPointerDevices++;
                REFIT_CALL_1_WRAPPER(gBS->Stall, 5 * 1000);
            }
        }
    } else {
        GlobalConfig.EnableMouse = FALSE;
    }

    if (NumAPointerDevices > 0 || NumSPointerDevices > 0) {
        REFIT_CALL_1_WRAPPER(gBS->Stall, 500000);
    }

    PointerAvailable = (NumAPointerDevices > 0 || NumSPointerDevices > 0);

    if (GlobalConfig.EnableMouse) {
        MouseImage = BuiltinIcon(BUILTIN_ICON_MOUSE);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Frees allocated memory and closes pointer protocols
////////////////////////////////////////////////////////////////////////////////
VOID pdCleanup() {
        #if REFIT_DEBUG > 0
        MsgLog ("Close Existing Pointer Protocols:\n");
        #endif

    PointerAvailable = FALSE;
    pdClear();

    if (APointerHandles) {
        UINTN Index;
        for (Index = 0; Index < NumAPointerDevices; Index++) {
            REFIT_CALL_4_WRAPPER(
                gBS->CloseProtocol,
                APointerHandles[Index],
                &APointerGuid,
                SelfImageHandle,
                NULL
            );
        }
        MyFreePool (&APointerHandles);
        APointerHandles = NULL;
    }
    if (APointerProtocol) {
        MyFreePool (&APointerProtocol);
        APointerProtocol = NULL;
    }
    if (SPointerHandles) {
        UINTN Index;
        for (Index = 0; Index < NumSPointerDevices; Index++) {
            REFIT_CALL_4_WRAPPER(
                gBS->CloseProtocol,
                SPointerHandles[Index],
                &SPointerGuid,
                SelfImageHandle,
                NULL
            );
        }
        MyFreePool (&SPointerHandles);
        SPointerHandles = NULL;
    }
    if (SPointerProtocol) {
        MyFreePool (&SPointerProtocol);
        SPointerProtocol = NULL;
    }
    if (MouseImage) {
        egFreeImage (MouseImage);
        Background = NULL;
    }
    NumAPointerDevices = 0;
    NumSPointerDevices = 0;

    LastXPos = ScreenW / 2;
    LastYPos = ScreenH / 2;

    State.X = ScreenW / 2;
    State.Y = ScreenH / 2;
    State.Press = FALSE;
    State.Holding = FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// Returns whether or not any pointer devices are available
////////////////////////////////////////////////////////////////////////////////
BOOLEAN pdAvailable() {
    return PointerAvailable;
}

////////////////////////////////////////////////////////////////////////////////
// Returns the number of pointer devices available
////////////////////////////////////////////////////////////////////////////////
UINTN pdCount() {
    return NumAPointerDevices + NumSPointerDevices;
}

////////////////////////////////////////////////////////////////////////////////
// Returns a pointer device's WaitForInput event
////////////////////////////////////////////////////////////////////////////////
EFI_EVENT pdWaitEvent (UINTN Index) {
    if (!PointerAvailable || Index >= NumAPointerDevices + NumSPointerDevices) {
        return NULL;
    }

    if (Index >= NumAPointerDevices) {
        return SPointerProtocol[Index - NumAPointerDevices]->WaitForInput;
    }
    return APointerProtocol[Index]->WaitForInput;
}

////////////////////////////////////////////////////////////////////////////////
// Gets the current state of all pointer devices and assigns State to
// the first available device's state
////////////////////////////////////////////////////////////////////////////////
// IMPORTANT: Ensure this declaration is at the top of your pointer.c file,
// outside any function (e.g., near other global variables like PointerAvailable).
// This makes LastHolding persist across function calls, which is crucial for State.Press.
static BOOLEAN LastHolding = FALSE;


EFI_STATUS pdUpdateState() {
#if defined (EFI32) && defined (__MAKEWITH_GNUEFI)
    return EFI_NOT_READY;
#else
    if (!PointerAvailable) {
        return EFI_NOT_READY;
    }

    EFI_STATUS Status = EFI_NOT_READY;
    EFI_ABSOLUTE_POINTER_STATE APointerState;
    EFI_SIMPLE_POINTER_STATE SPointerState;

    LastHolding = State.Holding;

    State.Holding = FALSE;
    State.Press = FALSE;

    UINTN Index;

    // Outer do-while loop to implement "first active device found, then break" logic from RefindPlus
    do {
        for (Index = 0; Index < NumAPointerDevices; Index++) {
            EFI_STATUS PointerStatus = REFIT_CALL_2_WRAPPER(
                APointerProtocol[Index]->GetState, // Using APointerProtocol from original
                APointerProtocol[Index],
                &APointerState
            );
            // If new state found (no error)
            if (!EFI_ERROR (PointerStatus)) {
                Status = EFI_SUCCESS; // Mark overall status as success due to this active absolute pointer

#ifdef EFI32
                State.X = (UINTN)DivU64x64Remainder (APointerState.CurrentX * ScreenW, APointerProtocol[Index]->Mode->AbsoluteMaxX, NULL);
                State.Y = (UINTN)DivU64x64Remainder (APointerState.CurrentY * ScreenH, APointerProtocol[Index]->Mode->AbsoluteMaxY, NULL);
#else
                State.X = (APointerState.CurrentX * ScreenW) / APointerProtocol[Index]->Mode->AbsoluteMaxX;
                State.Y = (APointerState.CurrentY * ScreenH) / APointerProtocol[Index]->Mode->AbsoluteMaxY;
#endif
                // Corrected: Accumulate State.Holding using OR
                State.Holding = State.Holding || (APointerState.ActiveButtons & EFI_ABSP_TouchActive);

                // Found an active absolute pointer, break from this for loop
                break;
            }
        }

        // If an absolute pointer was successfully processed, exit the outer do-while loop
        if (!EFI_ERROR(Status)) {
            break;
        }

        for (Index = 0; Index < NumSPointerDevices; Index++) {
            EFI_STATUS PointerStatus = REFIT_CALL_2_WRAPPER(
                SPointerProtocol[Index]->GetState, // Using SPointerProtocol from original
                SPointerProtocol[Index],
                &SPointerState
            );
            // If new state found (no error)
            if (!EFI_ERROR (PointerStatus)) {
                // If a simple pointer has movement or a button press, it should set overall Status to SUCCESS.
                // If Status is already SUCCESS from an absolute pointer, we don't overwrite it to EFI_NOT_READY.
                if (EFI_ERROR(Status) || (SPointerState.RelativeMovementX != 0 || SPointerState.RelativeMovementY != 0 || SPointerState.LeftButton || SPointerState.RightButton)) {
                    Status = EFI_SUCCESS;
                }

                INT32 TargetX = 0;
                INT32 TargetY = 0;

#ifdef EFI32
                TargetX = State.X + (INTN)DivS64x64Remainder (
                    SPointerState.RelativeMovementX * GlobalConfig.MouseSpeed,
                    SPointerProtocol[Index]->Mode->ResolutionX,
                    NULL
                );
                TargetY = State.Y + (INTN)DivS64x64Remainder (
                    SPointerState.RelativeMovementY * GlobalConfig.MouseSpeed,
                    SPointerProtocol[Index]->Mode->ResolutionY,
                    NULL
                );
#else
                TargetX = State.X + SPointerState.RelativeMovementX *
                    GlobalConfig.MouseSpeed / SPointerProtocol[Index]->Mode->ResolutionX;
                TargetY = State.Y + SPointerState.RelativeMovementY *
                    GlobalConfig.MouseSpeed / SPointerProtocol[Index]->Mode->ResolutionY;
#endif

                if (TargetX < 0) {
                    State.X = 0;
                }
                else if (TargetX >= ScreenW) {
                    State.X = ScreenW - 1;
                }
                else {
                    State.X = TargetX;
                }

                if (TargetY < 0) {
                    State.Y = 0;
                }
                else if (TargetY >= ScreenH) {
                    State.Y = ScreenH - 1;
                }
                else {
                    State.Y = TargetY;
                }

                // Corrected: Accumulate State.Holding using OR
                State.Holding = State.Holding || (SPointerState.LeftButton || SPointerState.RightButton);

                // Found an active simple pointer, break from this for loop
                break;
            }
        }
    } while (0); // This 'loop' only runs once, implementing the "first active device" logic

    State.Press = (!LastHolding && State.Holding); // Detects a BUTTON PRESS (button just went down)
    if (State.X != LastXPos || State.Y != LastYPos) { // Mouse has moved
        if (gSuppressPointerDraw) { // If pointer was suppressed (hidden)
            gSuppressPointerDraw = FALSE; // Show the pointer
        }
    }
    return Status;
#endif
}
////////////////////////////////////////////////////////////////////////////////
// Returns the current pointer state
////////////////////////////////////////////////////////////////////////////////
POINTER_STATE pdGetState() {
    return State;
}

////////////////////////////////////////////////////////////////////////////////
// Draw the mouse at the current coordinates
////////////////////////////////////////////////////////////////////////////////
VOID pdDraw() {
    if (gSuppressPointerDraw) {
        return;
    }

    if(Background != NULL) {
        egDrawImage(Background, LastXPos, LastYPos);
        egFreeImage(Background);
        Background = NULL;
    }

    if (MouseImage == NULL) {
        return;
    }

    UINTN Width  = MouseImage->Width;
    UINTN Height = MouseImage->Height;

    if(State.X + Width > ScreenW) {
        Width = ScreenW - State.X;
    }
    if(State.Y + Height > ScreenH) {
        Height = ScreenH - State.Y;
    }

    Background = egCopyScreenArea(State.X, State.Y, Width, Height);
    if(Background != NULL) { // Only attempt to draw if background was successfully captured
        BltImageCompositeBadge(Background, MouseImage, NULL, State.X, State.Y);
    }

    LastXPos = State.X;
    LastYPos = State.Y;
}

////////////////////////////////////////////////////////////////////////////////
// Restores the background at the position the mouse was last drawn
////////////////////////////////////////////////////////////////////////////////
VOID pdClear() {
    if (Background) {
        egDrawImage(Background, LastXPos, LastYPos);
        egFreeImage(Background);
        Background = NULL;
    }
}
