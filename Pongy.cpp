//-----------------------------------------------------------------------------
// File: Pongy.cpp
//
// Desc: A very simple (and nasty) pong game written using DirectDraw &
//       DirectInput
//
// Author: Alan 'Big Al' Cruikshanks
//-----------------------------------------------------------------------------
#define STRICT

#include <stdio.h>
#include <windows.h>
#include <ddraw.h>
#include <dinput.h>
#include <mmsystem.h>

#include "resource.h"
#include "ddutil.h"

//-----------------------------------------------------------------------------
// Defines and constants
//-----------------------------------------------------------------------------
#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

#define WINDOW_WIDTH			640
#define WINDOW_HEIGHT			480

#define BALL_SPRITE_DIAMETER	32

#define BAT_SPRITE_WIDTH		10
#define BAT_SPRITE_HEIGHT		75
#define BAT_EDGE_SPACER			10
#define COMPUTER_LEVEL			250.0

#define NUM_SPRITES				3

#define BALL_SPEED				5
#define BALL_SPEED_INC			50.0
#define BAT_SPEED				15

enum SpriteType {playerBat, computerBat, ball};
enum PlayerType {human, computer};

struct SPRITE_STRUCT
{
	SpriteType sType;
    FLOAT fPosX; 
    FLOAT fPosY;
    FLOAT fVelX; 
    FLOAT fVelY;
};

struct SCORE_STRUCT
{
	int nPlayerScore;
	int nComputerScore;
};

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
HWND					g_hMainWnd		= NULL;
CDisplay*				g_pDisplay		= NULL;
CSurface*				g_pBallSurface	= NULL;
CSurface*				g_pBatSurface	= NULL;  
CSurface*				g_pTextSurface	= NULL;
LPDIRECTINPUT8			g_pDI			= NULL;
LPDIRECTINPUTDEVICE8	g_pKeyboard		= NULL;
RECT					g_rcViewport;          
RECT					g_rcScreen;            
BOOL					g_bActive		= FALSE; 
DWORD					g_dwLastTick;
SPRITE_STRUCT			g_Sprite[NUM_SPRITES];
SCORE_STRUCT			g_Score;
PlayerType				whoseTurn;

//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
LRESULT CALLBACK MainWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
HRESULT WinInit( HINSTANCE hInst, int nCmdShow, HWND* phWnd, HACCEL* phAccel );
HRESULT InitDirectDraw();
HRESULT InitDirectInput( HINSTANCE hInst );
VOID	InitSprites();
HRESULT	ProcessIdle();
VOID	FreeDirectDraw();
BOOL	CleanUp();
HRESULT ProcessNextFrame();
VOID    UpdatePlayerBat( FLOAT fTimeDelta );
VOID    UpdateComputerBat( FLOAT fTimeDelta );
VOID    UpdateBall( FLOAT fTimeDelta );
VOID	UpdateScore( PlayerType pType );
HRESULT DisplayFrame();
HRESULT RestoreSurfaces();

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program. Initializes everything and calls
//       ProcessIdle() when idle from the message pump.
//-----------------------------------------------------------------------------
int APIENTRY WinMain( HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR pCmdLine, int nCmdShow )
{
    MSG		 msg;
    HACCEL   hAccel;

    srand( GetTickCount() );

    if( FAILED( WinInit( hInst, nCmdShow, &g_hMainWnd, &hAccel ) ) )
	{
		MessageBox( g_hMainWnd, TEXT("Window init failed. ")
                    TEXT("Pongy will now exit. "), TEXT("Pongy"), 
                    MB_ICONERROR | MB_OK );
        return CleanUp();
	}

    if( FAILED( InitDirectDraw() ) )
    {
        MessageBox( g_hMainWnd, TEXT("DirectDraw init failed. ")
                    TEXT("Pongy will now exit. "), TEXT("Pongy"), 
                    MB_ICONERROR | MB_OK );
        return CleanUp();
    }

	if( FAILED( InitDirectInput( hInst ) ) )
	{
        MessageBox( g_hMainWnd, TEXT("DirectInput init failed.  ")
                    TEXT("Pongy will now exit. "), TEXT("Pongy"), 
                    MB_ICONERROR | MB_OK );
        return CleanUp();
	}

 	InitSprites();

    g_dwLastTick = timeGetTime();

    while( TRUE )
    {
        // Look for messages, if none are found then 
        // update the state and display it
        if( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) )
        {
            if( 0 == GetMessage(&msg, NULL, 0, 0 ) )
            {
                // WM_QUIT was posted, so exit
                return (int)msg.wParam;
            }

            // Translate and dispatch the message
            if( 0 == TranslateAccelerator( g_hMainWnd, hAccel, &msg ) )
            {
                TranslateMessage( &msg ); 
                DispatchMessage( &msg );
            }
        }
        else
        {
			if( FAILED( ProcessIdle() ) )
			{
				MessageBox( g_hMainWnd, TEXT("Displaying the next frame failed. ")
							TEXT("Pongy will now exit. "), TEXT("Pongy"), 
							MB_ICONERROR | MB_OK );
				return CleanUp();
			}
        }
    }
}

//-----------------------------------------------------------------------------
// Name: WinInit()
// Desc: Init the window
//-----------------------------------------------------------------------------
HRESULT WinInit( HINSTANCE hInst, int nCmdShow, HWND* phWnd, HACCEL* phAccel )
{
    WNDCLASSEX wc;
    HWND       hWnd;
    HACCEL     hAccel;

    // Register the Window Class
    wc.cbSize        = sizeof(wc);
    wc.lpszClassName = TEXT("Pongy");
    wc.lpfnWndProc   = MainWndProc;
    wc.style         = CS_VREDRAW | CS_HREDRAW;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon( hInst, MAKEINTRESOURCE(IDI_MAIN) );
    wc.hIconSm       = LoadIcon( hInst, MAKEINTRESOURCE(IDI_MAIN) );
    wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU);
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;

    if( RegisterClassEx( &wc ) == 0 )
        return E_FAIL;

    // Load keyboard accelerators
    hAccel = LoadAccelerators( hInst, MAKEINTRESOURCE(IDR_MAIN_ACCEL) );

    // Calculate the proper size for the window given a client of 640x480
    DWORD dwFrameWidth    = GetSystemMetrics( SM_CXSIZEFRAME );
    DWORD dwFrameHeight   = GetSystemMetrics( SM_CYSIZEFRAME );
    DWORD dwMenuHeight    = GetSystemMetrics( SM_CYMENU );
    DWORD dwCaptionHeight = GetSystemMetrics( SM_CYCAPTION );
    DWORD dwWindowWidth   = WINDOW_WIDTH  + dwFrameWidth * 2;
    DWORD dwWindowHeight  = WINDOW_HEIGHT + dwFrameHeight * 2 + 
                            dwMenuHeight + dwCaptionHeight;

    // Create and show the main window
    DWORD dwStyle = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX;
    hWnd = CreateWindowEx( 0, TEXT("Pongy"), TEXT("Pongy"),
                           dwStyle, CW_USEDEFAULT, CW_USEDEFAULT,
  	                       dwWindowWidth, dwWindowHeight, NULL, NULL, hInst, NULL );
    if( hWnd == NULL )
    	return E_FAIL;

    ShowWindow( hWnd, nCmdShow );
    UpdateWindow( hWnd );

    *phWnd   = hWnd;
    *phAccel = hAccel;

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: InitDirectDraw()
// Desc: Create the DirectDraw object, and init the surfaces
//-----------------------------------------------------------------------------
HRESULT InitDirectDraw()
{
    HRESULT	hr;

    g_pDisplay = new CDisplay();
    if( FAILED( hr = g_pDisplay->CreateWindowedDisplay( g_hMainWnd, WINDOW_WIDTH, WINDOW_HEIGHT ) ) )
        return hr;

	// Create a ball surface, and draw a ball bitmap resource on it.  
    if( FAILED( hr = g_pDisplay->CreateSurfaceFromBitmap( &g_pBallSurface, MAKEINTRESOURCE( IDB_BALL ), 
                                                          BALL_SPRITE_DIAMETER, BALL_SPRITE_DIAMETER ) ) )
        return hr;

	// Create a bat surface, and draw a bat bitmap resource on it.  
    if( FAILED( hr = g_pDisplay->CreateSurfaceFromBitmap( &g_pBatSurface, MAKEINTRESOURCE( IDB_BAT ), 
                                                          BAT_SPRITE_WIDTH, BAT_SPRITE_HEIGHT ) ) )
        return hr;

	// Create a surface, and draw text to it.
	TCHAR scoreMsg[MAX_PATH] = TEXT("");
	sprintf( scoreMsg, TEXT("YOU %d - %d CMP"), g_Score.nPlayerScore, g_Score.nComputerScore);

	if( FAILED( hr = g_pDisplay->CreateSurfaceFromText( &g_pTextSurface, NULL, scoreMsg, 
                                                        RGB(0,0,0), RGB(255, 255, 0) ) ) )
        return hr;

    // Set the color key for the ball sprite to black
    if( FAILED( hr = g_pBallSurface->SetColorKey( 0 ) ) )
        return hr;

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: InitDirectInput()
// Desc: Initialise the DirectInput objects
//-----------------------------------------------------------------------------
HRESULT InitDirectInput( HINSTANCE hInst )
{
	HRESULT         hr; 

	// Create a IDirectInput8*
	if( FAILED( hr = DirectInput8Create( hInst, DIRECTINPUT_VERSION, 
										IID_IDirectInput8, (void**)&g_pDI, NULL ) ) )
		return hr;

	// Create a IDirectInputDevice8* for the keyboard
    if( FAILED( hr = g_pDI->CreateDevice( GUID_SysKeyboard, &g_pKeyboard, NULL ) ) )
        return hr;

	// Set the keyboard data format
    if( FAILED( hr = g_pKeyboard->SetDataFormat( &c_dfDIKeyboard ) ) )
        return hr;

	// Set the cooperative level on the keyboard
    if( FAILED( hr = g_pKeyboard->SetCooperativeLevel( g_hMainWnd, 
														DISCL_NONEXCLUSIVE | 
														DISCL_FOREGROUND | 
														DISCL_NOWINKEY ) ) )
        return hr;

	if (g_pKeyboard) g_pKeyboard->Acquire();

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: InitSprites()
// Desc: Initialises each of the sprites
//-----------------------------------------------------------------------------
VOID InitSprites()
{
	ZeroMemory( &g_Sprite, sizeof(SPRITE_STRUCT) * NUM_SPRITES );

	// Set the ball sprite
	g_Sprite[0].sType = ball;

	// Set the position and velocity
	g_Sprite[0].fPosX = (float) ((WINDOW_WIDTH / 2) - (BALL_SPRITE_DIAMETER / 2));
	g_Sprite[0].fPosY = (float) (0);

	// Keep changing the x velocity until speed is realistic
	while (1)
	{
		g_Sprite[0].fVelX = 500.0f * rand() / RAND_MAX - 250.0f;
		g_Sprite[0].fVelY = 500.0f * rand() / RAND_MAX - 250.0f;
		if( g_Sprite[0].fVelX > 100.0 || g_Sprite[0].fVelX < -100.0)
			if( g_Sprite[0].fVelY > 50.0 || g_Sprite[0].fVelY < -50.0)
				break;
	}
	if( g_Sprite[0].fVelX >= 0)
		whoseTurn = computer;
	else
		whoseTurn = human;


	// Set the player bat sprite
	g_Sprite[1].sType = playerBat;

    // Set the position and velocity
    g_Sprite[1].fPosX = (float) (BAT_EDGE_SPACER);
    g_Sprite[1].fPosY = (float) ((WINDOW_HEIGHT / 2) - (BAT_SPRITE_HEIGHT / 2)); 

    g_Sprite[1].fVelX = 0;
    g_Sprite[1].fVelY = 500.0f * BAT_SPEED / RAND_MAX - 250.0f;


	// Set the computer bat sprite
	g_Sprite[2].sType = computerBat;

    // Set the position and velocity
    g_Sprite[2].fPosX = (float) (WINDOW_WIDTH - (BAT_SPRITE_WIDTH + BAT_EDGE_SPACER));
    g_Sprite[2].fPosY = (float) ((WINDOW_HEIGHT / 2) - (BAT_SPRITE_HEIGHT / 2)); 

    g_Sprite[2].fVelX = 0;
    g_Sprite[2].fVelY = 500.0f * BAT_SPEED / RAND_MAX - 250.0f;
}

//-----------------------------------------------------------------------------
// Name: ProcessIdle()
// Desc: Performs the actual program operation, updating the 
//       state & displaying it.
//-----------------------------------------------------------------------------
HRESULT ProcessIdle()
{
	HRESULT hr;

	if( g_bActive )
	{
		// Move the sprites, blt them to the back buffer, then 
		// flip or blt the back buffer to the primary buffer
		if( FAILED( hr = ProcessNextFrame() ) )
		{
			if( hr = DDERR_SURFACELOST )
			{
				 // The surfaces were lost so restore them 
				if( FAILED( hr = RestoreSurfaces() ) )
					return hr;
			}
			else
				return hr;
		}
	}
	else
	{
		// Make sure we go to sleep if we have nothing else to do
		WaitMessage();

		// Ignore time spent inactive 
		g_dwLastTick = timeGetTime();
	}

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: MainWndProc()
// Desc: The main window procedure
//-----------------------------------------------------------------------------
LRESULT CALLBACK MainWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch (msg)
    {
        case WM_COMMAND:
            switch( LOWORD(wParam) )
            {
                case IDM_EXIT:
                    // Received key/menu command to exit app
            	    PostMessage( hWnd, WM_CLOSE, 0, 0 );
                    return 0L;
            }
            break; // Continue with default processing

        case WM_PAINT:
            // Update the screen if we need to refresh. This case occurs 
            // when in windowed mode and the window is behind others.
            // The app will not be active, but it will be visible.
            if( g_pDisplay )
            {
                // Display the new position of the sprite
                if( DisplayFrame() == DDERR_SURFACELOST )
                {
                    // If the surfaces were lost, then restore and try again
                    RestoreSurfaces();
                    DisplayFrame();
                }
            }
            break; // Continue with default processing to validate the region

        case WM_QUERYNEWPALETTE:
            if( g_pDisplay )
            {
                // If we are in windowed mode with a desktop resolution in 8 bit 
                // color, then the palette we created during init has changed 
                // since then.  So get the palette back from the primary 
                // DirectDraw surface, and set it again so that DirectDraw 
                // realises the palette, then release it again. 
                LPDIRECTDRAWPALETTE pDDPal = NULL; 
                g_pDisplay->GetFrontBuffer()->GetPalette( &pDDPal );
                g_pDisplay->GetFrontBuffer()->SetPalette( pDDPal );
                SAFE_RELEASE( pDDPal );
            }
            break;

        case WM_GETMINMAXINFO:
            {
                // Don't allow resizing in windowed mode.  
                // Fix the size of the window to 640x480 (client size)
                MINMAXINFO* pMinMax = (MINMAXINFO*) lParam;

                DWORD dwFrameWidth    = GetSystemMetrics( SM_CXSIZEFRAME );
                DWORD dwFrameHeight   = GetSystemMetrics( SM_CYSIZEFRAME );
                DWORD dwMenuHeight    = GetSystemMetrics( SM_CYMENU );
                DWORD dwCaptionHeight = GetSystemMetrics( SM_CYCAPTION );

                pMinMax->ptMinTrackSize.x = WINDOW_WIDTH  + dwFrameWidth * 2;
                pMinMax->ptMinTrackSize.y = WINDOW_HEIGHT + dwFrameHeight * 2 + 
                                            dwMenuHeight + dwCaptionHeight;

                pMinMax->ptMaxTrackSize.x = pMinMax->ptMinTrackSize.x;
                pMinMax->ptMaxTrackSize.y = pMinMax->ptMinTrackSize.y;
            }
            return 0L;

        case WM_MOVE:
	        if( g_pDisplay )
		        g_pDisplay->UpdateBounds();
            return 0L;

        case WM_EXITMENULOOP:
            // Ignore time spent in menu
            g_dwLastTick = timeGetTime();
            break;

        case WM_EXITSIZEMOVE:
            // Ignore time spent resizing
            g_dwLastTick = timeGetTime();
            break;

        case WM_SIZE:
            // Check to see if we are losing our window...
            if( SIZE_MAXHIDE==wParam || SIZE_MINIMIZED==wParam )
                g_bActive = FALSE;
            else
                g_bActive = TRUE;

			if( g_pDisplay )
		        g_pDisplay->UpdateBounds();
            break;
            
        case WM_DESTROY:
            // Cleanup and close the app
            CleanUp();
            PostQuitMessage( 0 );
            return 0L;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

//-----------------------------------------------------------------------------
// Name: ProcessNextFrame()
// Desc: Move the sprites, blt them to the back buffer, then 
//       flip or blt the back buffer to the primary buffer
//-----------------------------------------------------------------------------
HRESULT ProcessNextFrame()
{
    HRESULT hr;

    // Figure how much time has passed since the last time
    DWORD dwCurrTick = timeGetTime();
    DWORD dwTickDiff = dwCurrTick - g_dwLastTick;

    // Don't update if no time has passed 
    if( dwTickDiff == 0 )
        return S_OK; 

    g_dwLastTick = dwCurrTick;

    // Move the sprites according their type & how much time has passed
	for( int i = 0; i < NUM_SPRITES; i++ )
	{
		switch( g_Sprite[i].sType )
		{
			case playerBat:
				UpdatePlayerBat( dwTickDiff / 1000.0f );
				break;

			case computerBat:
				UpdateComputerBat( dwTickDiff / 1000.0f );
				break;

			case ball:
				UpdateBall( dwTickDiff / 1000.0f );
				break;
		}
	}

    // Check the cooperative level before rendering
    if( FAILED( hr = g_pDisplay->GetDirectDraw()->TestCooperativeLevel() ) )
    {
        switch( hr )
        {
            case DDERR_EXCLUSIVEMODEALREADYSET:
                // Do nothing because some other app has exclusive mode
                Sleep(10);
                return S_OK;

            case DDERR_WRONGMODE:

                // The display mode changed on us. Update the
                // DirectDraw surfaces accordingly
                FreeDirectDraw();
				InitSprites();
                return InitDirectDraw();
        }
        return hr;
    }

    // Display the sprites on the screen
    if( FAILED( hr = DisplayFrame() ) )
    {
        if( hr != DDERR_SURFACELOST )
            return hr;
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: UpdatePlayerBat()
// Desc: Move the players bat around and make it bounce based on how much time 
//       has passed
//-----------------------------------------------------------------------------
VOID UpdatePlayerBat( FLOAT fTimeDelta )
{
	#define KEYDOWN(name, key) (name[key] & 0x80) 
 
    char     buffer[256]; 
    HRESULT  hr;
 
	if (g_pKeyboard) g_pKeyboard->Acquire();
    if( FAILED( hr = g_pKeyboard->GetDeviceState(sizeof(buffer),(LPVOID)&buffer) ) )
	{
         // If it failed, the device has probably been lost. 
         // Check for (hr == DIERR_INPUTLOST) and attempt to reacquire it here.
		if( hr == DIERR_INPUTLOST ) 
		{
			if (g_pKeyboard) g_pKeyboard->Acquire();
		}
		else
		{
			MessageBox( g_hMainWnd, TEXT("Update player failed. ")
						TEXT("Pongy will now exit. "), TEXT("Pongy"), 
					    MB_ICONERROR | MB_OK );
			CleanUp();
			exit(0);
		}
    } 
 
    // Update the player bat position
    if (KEYDOWN(buffer, DIK_UP))
	{
		g_Sprite[1].fPosY += g_Sprite[1].fVelY * fTimeDelta;
	}	
    else if (KEYDOWN(buffer, DIK_DOWN))
	{
		g_Sprite[1].fPosY -= g_Sprite[1].fVelY * fTimeDelta;
	}

	// Check bat not going beyond screen borders
    if( g_Sprite[1].fPosY < 0 )
    {
        g_Sprite[1].fPosY = 0;
    }

    if( g_Sprite[1].fPosY > WINDOW_HEIGHT - BAT_SPRITE_HEIGHT )
    {
        g_Sprite[1].fPosY = WINDOW_HEIGHT - 1 - BAT_SPRITE_HEIGHT;
    }   
}

//-----------------------------------------------------------------------------
// Name: UpdateComputerBat()
// Desc: Move the sprite around and make it bounce based on how much time 
//       has passed
//-----------------------------------------------------------------------------
VOID UpdateComputerBat( FLOAT fTimeDelta )
{    
	// Computer will not move until player has hit ball
	if((whoseTurn == human) || (g_Sprite[0].fPosX < COMPUTER_LEVEL))
		return;

	// Update the computers bat position based on ball position
	if(g_Sprite[2].fPosY < g_Sprite[0].fPosY)
		g_Sprite[2].fPosY -= g_Sprite[2].fVelY * fTimeDelta;

	if(g_Sprite[2].fPosY > g_Sprite[0].fPosY)
		g_Sprite[2].fPosY += g_Sprite[2].fVelY * fTimeDelta;

	// Check bat not going beyond screen borders
    if( g_Sprite[2].fPosY < 0 )
    {
        g_Sprite[2].fPosY = 0;
    }

    if( g_Sprite[2].fPosY > WINDOW_HEIGHT - BAT_SPRITE_HEIGHT )
    {
        g_Sprite[2].fPosY = WINDOW_HEIGHT - 1 - BAT_SPRITE_HEIGHT;
    }   
}

//-----------------------------------------------------------------------------
// Name: UpdateBall()
// Desc: Move the ball sprite around and make it bounce based on how much time 
//       has passed
//-----------------------------------------------------------------------------
VOID UpdateBall( FLOAT fTimeDelta )
{    
	PlayerType type;

    // Update the sprite position
    g_Sprite[0].fPosX += g_Sprite[0].fVelX * fTimeDelta;
    g_Sprite[0].fPosY += g_Sprite[0].fVelY * fTimeDelta;

    // Check if computer scored a point
    if( g_Sprite[0].fPosX < 0.0f )
    {
		type = computer;
		UpdateScore(type);
		InitSprites();

 		return;
    }

	// Check if player scored a point
    if( g_Sprite[0].fPosX >= WINDOW_WIDTH - BALL_SPRITE_DIAMETER )
    {
		type = human;
		UpdateScore(type);
		InitSprites();

		return;
    }

	// Bounce the ball if it hits the top or bottom
    if( g_Sprite[0].fPosY < 0 )
    {
        g_Sprite[0].fPosY = 0;
        g_Sprite[0].fVelY = -g_Sprite[0].fVelY;
		return;
    }

    if( g_Sprite[0].fPosY > WINDOW_HEIGHT - BALL_SPRITE_DIAMETER )
    {
        g_Sprite[0].fPosY = WINDOW_HEIGHT - 1 - BALL_SPRITE_DIAMETER;
        g_Sprite[0].fVelY = -g_Sprite[0].fVelY;
		return;
    }

	// Bounce the ball if it hit the players bat
	if( g_Sprite[0].fPosX <= (g_Sprite[1].fPosX + BAT_SPRITE_WIDTH))
	{
		if( (g_Sprite[0].fPosY + BALL_SPRITE_DIAMETER >= g_Sprite[1].fPosY) &&
			(g_Sprite[0].fPosY <= g_Sprite[1].fPosY + BAT_SPRITE_HEIGHT ) )
		{
			g_Sprite[0].fPosX  = (float) (BAT_EDGE_SPACER + BAT_SPRITE_WIDTH);
			g_Sprite[0].fVelX = -g_Sprite[0].fVelX;
			
			// Up the ball speed
			g_Sprite[0].fVelX += BALL_SPEED_INC;

			// Inform computer that its it's turn
			whoseTurn = computer;
			return;
		}
	}

	// Bounce the ball if it hit the computers bat
	if( g_Sprite[0].fPosX + BALL_SPRITE_DIAMETER >= g_Sprite[2].fPosX)
	{
		if( (g_Sprite[0].fPosY + BALL_SPRITE_DIAMETER >= g_Sprite[2].fPosY) &&
			(g_Sprite[0].fPosY <= g_Sprite[2].fPosY + BAT_SPRITE_HEIGHT ) )
		{
			g_Sprite[0].fPosX = (float) (WINDOW_WIDTH - (BAT_EDGE_SPACER + BAT_SPRITE_WIDTH + BALL_SPRITE_DIAMETER));
			g_Sprite[0].fVelX = -g_Sprite[0].fVelX;

			// Up the ball speed
			g_Sprite[0].fVelX -= BALL_SPEED_INC;

			// Inform player that its their turn
			whoseTurn = human;
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Name: UpdateScore()
// Desc: Updates the current score and the score text surface
//-----------------------------------------------------------------------------
VOID UpdateScore(PlayerType type)
{
	HRESULT hr;

	switch(type)
	{
		case human:
			g_Score.nPlayerScore += 1;
			break;
		case computer:
			g_Score.nComputerScore += 1;
			break;
	}

	// Update the score text surface.
	TCHAR scoreMsg[MAX_PATH] = TEXT("");
	sprintf( scoreMsg, TEXT("YOU %d - %d CMP"), g_Score.nPlayerScore, g_Score.nComputerScore);
	
	if(g_Score.nPlayerScore > 9 || g_Score.nComputerScore > 9)
	{
		SAFE_DELETE( g_pTextSurface );
		if( FAILED( hr = g_pDisplay->CreateSurfaceFromText( &g_pTextSurface, NULL, scoreMsg, 
										                  RGB(0,0,0), RGB(255, 255, 0) ) ) )
		{
			MessageBox( g_hMainWnd, TEXT("Update score failed. ")
						TEXT("Pongy will now exit. "), TEXT("Pongy"), 
						MB_ICONERROR | MB_OK );
			CleanUp();
			exit(0);
		}
	}
	else
		g_pTextSurface->DrawText( NULL, scoreMsg, 0, 0, RGB(0,0,0), RGB(255, 255, 0) );
}

//-----------------------------------------------------------------------------
// Name: DisplayFrame()
// Desc: Blts a the sprites to the back buffer, then it blts or flips the 
//       back buffer onto the primary buffer.
//-----------------------------------------------------------------------------
HRESULT DisplayFrame()
{
    HRESULT hr;

	// Fill the back buffer with black, ignoring errors until the flip
    g_pDisplay->Clear( 0 );

	// Blt the score text on the backbuffer, ignoring errors until the flip
	int msgPosX = ((WINDOW_WIDTH / 2) - 50);
    g_pDisplay->Blt( msgPosX, 10, g_pTextSurface, NULL );

    // Blt all the sprites onto the back buffer using color keying,
    // ignoring errors until the last blt. Note that all of these sprites 
    // use the same DirectDraw surface.
    for( int i = 0; i < NUM_SPRITES; i++ )
    {
		if( g_Sprite[i].sType == ball)
		{
			g_pDisplay->Blt( (DWORD)g_Sprite[i].fPosX, 
							(DWORD)g_Sprite[i].fPosY, 
							g_pBallSurface, NULL );
		}
		else
		{
			g_pDisplay->Blt( (DWORD)g_Sprite[i].fPosX, 
                         (DWORD)g_Sprite[i].fPosY, 
                         g_pBatSurface, NULL );
		}
    }

    // We are in windowed mode so perform a blt from the backbuffer 
    // to the primary, returning any errors like DDERR_SURFACELOST
    if( FAILED( hr = g_pDisplay->Present() ) )
        return hr;

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: RestoreSurfaces()
// Desc: Restore all the surfaces, and redraw the sprite surfaces.
//-----------------------------------------------------------------------------
HRESULT RestoreSurfaces()
{
    HRESULT hr;

	if( FAILED( hr = g_pDisplay->GetDirectDraw()->RestoreAllSurfaces() ) )
        return hr;

 	// No need to re-create the surface, just re-draw it.
    if( FAILED( hr = g_pBallSurface->DrawBitmap( MAKEINTRESOURCE( IDB_BALL ),
												BAT_SPRITE_WIDTH, BALL_SPRITE_DIAMETER ) ) )
        return hr;

	// No need to re-create the surface, just re-draw it.
	TCHAR scoreMsg[MAX_PATH] = TEXT("");
	sprintf( scoreMsg, TEXT("YOU %d - %d CMP"), g_Score.nPlayerScore, g_Score.nComputerScore);

    if( FAILED( hr = g_pTextSurface->DrawText( NULL, scoreMsg, 
                                               0, 0, RGB(0,0,0), RGB(255, 255, 0) ) ) )
        return hr;

    // No need to re-create the surface, just re-draw it.
    if( FAILED( hr = g_pBatSurface->DrawBitmap( MAKEINTRESOURCE( IDB_BAT ),
                                                BAT_SPRITE_WIDTH, BAT_SPRITE_HEIGHT ) ) )
        return hr;

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: CleanUp()
// Desc: Releases all DirectX objects
//-----------------------------------------------------------------------------
BOOL CleanUp()
{
	FreeDirectDraw();

    if (g_pDI) 
    { 
        if (g_pKeyboard) 
        { 
			// Always unacquire device before calling Release(). 
            g_pKeyboard->Unacquire(); 
            g_pKeyboard->Release();
            g_pKeyboard = NULL; 
        } 
        g_pDI->Release();
        g_pDI = NULL; 
    }

	SAFE_DELETE( g_pBallSurface );
	SAFE_DELETE( g_pBatSurface );
    SAFE_DELETE( g_pTextSurface );
    SAFE_DELETE( g_pDisplay );

	return FALSE;
}

//-----------------------------------------------------------------------------
// Name: FreeDirectDraw()
// Desc: Release all the DirectDraw objects
//-----------------------------------------------------------------------------
VOID FreeDirectDraw()
{
    g_pBallSurface->Destroy();
	g_pBatSurface->Destroy();
    g_pTextSurface->Destroy();
    g_pDisplay->DestroyObjects();
}




