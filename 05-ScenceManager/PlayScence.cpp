
#include <iostream>
#include <fstream>
#include<stdio.h>
#include<stdlib.h>

#include "PlayScence.h"
#include "Utils.h"
#include "Textures.h"
#include "Sprites.h"
#include "Portal.h"
#include "Terrain.h"

using namespace std;

CPlayScene::CPlayScene(int id, LPCWSTR filePath):
	CScene(id, filePath)
{
	key_handler = new CPlayScenceKeyHandler(this);
}

/*
	Load scene resources from scene file (textures, sprites, animations and objects)
	See scene1.txt, scene2.txt for detail format specification
*/

#define SCENE_SECTION_UNKNOWN -1
#define SCENE_SECTION_TEXTURES 2
#define SCENE_SECTION_SPRITES 3
#define SCENE_SECTION_ANIMATIONS 4
#define SCENE_SECTION_ANIMATION_SETS	5
#define SCENE_SECTION_OBJECTS	6
#define SCENE_SECTION_TERRAIN	7

/*#define OBJECT_TYPE_MARIO	0
#define OBJECT_TYPE_BRICK	1
#define OBJECT_TYPE_GOOMBA	2
#define OBJECT_TYPE_KOOPAS	3
#define OBJECT_TYPE_BOUNTYBRICK	4*/


#define OBJECT_TYPE_PORTAL	50

#define MAX_SCENE_LINE 1024


void CPlayScene::_ParseSection_TEXTURES(string line)
{
	vector<string> tokens = split(line);

	if (tokens.size() < 5) return; // skip invalid lines

	int texID = atoi(tokens[0].c_str());
	wstring path = ToWSTR(tokens[1]);
	int R = atoi(tokens[2].c_str());
	int G = atoi(tokens[3].c_str());
	int B = atoi(tokens[4].c_str());

	CTextures::GetInstance()->Add(texID, path.c_str(), D3DCOLOR_XRGB(R, G, B));
}

void CPlayScene::_ParseSection_SPRITES(string line)
{
	vector<string> tokens = split(line);

	if (tokens.size() < 6) return; // skip invalid lines

	int ID = atoi(tokens[0].c_str());
	int l = atoi(tokens[1].c_str());
	int t = atoi(tokens[2].c_str());
	int r = atoi(tokens[3].c_str());
	int b = atoi(tokens[4].c_str());
	int texID = atoi(tokens[5].c_str());

	LPDIRECT3DTEXTURE9 tex = CTextures::GetInstance()->Get(texID);
	if (tex == NULL)
	{
		DebugOut(L"[ERROR] Texture ID %d not found!\n", texID);
		return; 
	}

	CSprites::GetInstance()->Add(ID, l, t, r, b, tex);
}

void CPlayScene::_ParseSection_ANIMATIONS(string line)
{
	vector<string> tokens = split(line);

	if (tokens.size() < 3) return; // skip invalid lines - an animation must at least has 1 frame and 1 frame time

	//DebugOut(L"--> %s\n",ToWSTR(line).c_str());

	LPANIMATION ani = new CAnimation();

	int ani_id = atoi(tokens[0].c_str());
	for (int i = 1; i < tokens.size(); i += 2)	// why i+=2 ?  sprite_id | frame_time  
	{
		int sprite_id = atoi(tokens[i].c_str());
		int frame_time = atoi(tokens[i+1].c_str());
		ani->Add(sprite_id, frame_time);
	}

	CAnimations::GetInstance()->Add(ani_id, ani);
}

void CPlayScene::_ParseSection_ANIMATION_SETS(string line)
{
	vector<string> tokens = split(line);

	if (tokens.size() < 2) return; // skip invalid lines - an animation set must at least id and one animation id

	int ani_set_id = atoi(tokens[0].c_str());

	LPANIMATION_SET s = new CAnimationSet();

	CAnimations *animations = CAnimations::GetInstance();

	for (int i = 1; i < tokens.size(); i++)
	{
		int ani_id = atoi(tokens[i].c_str());
		
		LPANIMATION ani = animations->Get(ani_id);
		s->push_back(ani);
	}

	CAnimationSets::GetInstance()->Add(ani_set_id, s);
}

/*
	Parse a line in section [OBJECTS] 
*/
void CPlayScene::_ParseSection_OBJECTS(string line)
{
	vector<string> tokens = split(line);

	//DebugOut(L"--> %s\n",ToWSTR(line).c_str());

	if (tokens.size() < 4) return; // skip invalid lines - an object set must have at least id, x, y

	int object_type = atoi(tokens[0].c_str());
	float x = atof(tokens[1].c_str());
	float y = atof(tokens[2].c_str());

	int ani_set_id = atoi(tokens[3].c_str());

	CAnimationSets * animation_sets = CAnimationSets::GetInstance();

	CGameObject *obj = NULL;
	switch (object_type)
	{
	case OBJECT_TYPE_MARIO:
		if (player!=NULL) 
		{
			DebugOut(L"[ERROR] MARIO object was created before!\n");
			return;
		}
		obj = new CMario(x,y); 
		player = (CMario*)obj;  

		DebugOut(L"[INFO] Player object created!\n");
		break;
	case OBJECT_TYPE_GOOMBA: obj = new CGoomba(); break;
	case OBJECT_TYPE_BRICK: obj = new CBrick(); break;
	case OBJECT_TYPE_KOOPAS: obj = new CKoopas(); break;
	case OBJECT_TYPE_BOUNTYBRICK: obj = new CBountyBrick(); break;
	case OBJECT_TYPE_HIDDENOBJECT: 
	{
		float r = atof(tokens[4].c_str());
		float b = atof(tokens[5].c_str());
		obj = new CHiddenObject(x, y, r, b);
	}
	break;
	case OBJECT_TYPE_PORTAL:
		{	
			float r = atof(tokens[4].c_str());
			float b = atof(tokens[5].c_str());
			int scene_id = atoi(tokens[6].c_str());
			obj = new CPortal(x, y, r, b, scene_id);
		}
		break;
	default:
		DebugOut(L"[ERR] Invalid object type: %d\n", object_type);
		return;
	}
	obj->SetType(object_type);
	// General object setup
	obj->SetPosition(x, y);
	LPANIMATION_SET ani_set = animation_sets->Get(ani_set_id);
	obj->SetAnimationSet(ani_set);
	objects.push_back(obj);
}
void CPlayScene::_ParseSection_TERRAIN(string line)
{
	vector<string> tokens = split(line);

	if (tokens.size() < 3) return;
	float x = atof(tokens[0].c_str());
	float y = atof(tokens[1].c_str());
	int sprite_id = atoi(tokens[2].c_str());
	CTerrain* terr = new CTerrain();

	terr->SetPosition(x, y);
	LPSPRITE sprites = CSprites::GetInstance()->Get(sprite_id);
	terr->SetSprite(sprites);
	terrains.push_back(terr);
}

void CPlayScene::Load()
{
	DebugOut(L"[INFO] Start loading scene resources from : %s \n", sceneFilePath);

	ifstream f;
	f.open(sceneFilePath);

	// current resource section flag
	int section = SCENE_SECTION_UNKNOWN;					

	char str[MAX_SCENE_LINE];
	while (f.getline(str, MAX_SCENE_LINE))
	{
		string line(str);

		if (line[0] == '#') continue;	// skip comment lines	

		if (line == "[TEXTURES]") { section = SCENE_SECTION_TEXTURES; continue; }
		if (line == "[SPRITES]") { 
			section = SCENE_SECTION_SPRITES; continue; }
		if (line == "[ANIMATIONS]") { 
			section = SCENE_SECTION_ANIMATIONS; continue; }
		if (line == "[ANIMATION_SETS]") { 
			section = SCENE_SECTION_ANIMATION_SETS; continue; }
		if (line == "[OBJECTS]") { 
			section = SCENE_SECTION_OBJECTS; continue; }
		if (line == "[TERRAINS]"){
			section = SCENE_SECTION_TERRAIN; continue;
		}
		if (line[0] == '[') { section = SCENE_SECTION_UNKNOWN; continue; }	

		//
		// data section
		//
		switch (section)
		{ 
			case SCENE_SECTION_TEXTURES: _ParseSection_TEXTURES(line); break;
			case SCENE_SECTION_SPRITES: _ParseSection_SPRITES(line); break;
			case SCENE_SECTION_ANIMATIONS: _ParseSection_ANIMATIONS(line); break;
			case SCENE_SECTION_ANIMATION_SETS: _ParseSection_ANIMATION_SETS(line); break;
			case SCENE_SECTION_OBJECTS: _ParseSection_OBJECTS(line);break;
			case SCENE_SECTION_TERRAIN: _ParseSection_TERRAIN(line); break;
		}
	}

	f.close();

	CTextures::GetInstance()->Add(ID_TEX_BBOX, L"textures\\bbox.png", D3DCOLOR_XRGB(255, 255, 255));

	DebugOut(L"[INFO] Done loading scene resources %s\n", sceneFilePath);
}

void CPlayScene::Update(DWORD dt)
{
	// We know that Mario is the first object in the list hence we won't add him into the colliable object list
	// TO-DO: This is a "dirty" way, need a moref organized way 

	vector<LPGAMEOBJECT> coObjects;
	for (size_t i = 1; i < objects.size(); i++)
	{
		coObjects.push_back(objects[i]);
	}

	for (size_t i = 0; i < objects.size(); i++)
	{
		objects[i]->Update(dt, &coObjects);
	}

	// skip the rest if scene was already unloaded (Mario::Update might trigger PlayScene::Unload)
	if (player == NULL) return; 

	// Update camera to follow mario
	float cx, cy;
	
	player->GetPosition(cx, cy);
	
	CGame *game = CGame::GetInstance();
	if (cx > game->GetScreenWidth() / 2)
	{
		CGame::GetInstance()->SetCamPos(round(cx-game->GetScreenWidth()/2),-game->GetScreenHeight());
	}
	/*else if (py % game->GetScreenHeight() > game->GetScreenHeight() / 6 * 5)
	{
		CGame::GetInstance()->SetCamPos(0, py % game->GetScreenHeight() - game->GetScreenHeight() / 6 * 5);
	}*/
	else
		CGame::GetInstance()->SetCamPos(0, -game->GetScreenHeight());	
}

void CPlayScene::Render()
{
	for (int i = 0; i < terrains.size(); i++)
		terrains[i]->Draw(terrains[i]->GetPositionX(),terrains[i]->GetPositionY(),255);
	for (int i = 0; i < objects.size(); i++)
		objects[i]->Render();
	
}

/*
	Unload current scene
*/
void CPlayScene::Unload()
{
	for (int i = 0; i < objects.size(); i++)
		delete objects[i];

	objects.clear();
	player = NULL;

	DebugOut(L"[INFO] Scene %s unloaded! \n", sceneFilePath);
}

void CPlayScenceKeyHandler::OnKeyDown(int KeyCode)
{
	//DebugOut(L"[INFO] KeyDown: %d\n", KeyCode);

	CMario *mario = ((CPlayScene*)scence)->GetPlayer();
	switch (KeyCode)
	{
	case DIK_SPACE:
		mario->SetState(MARIO_STATE_JUMP);
		break;
	case DIK_A: 
		mario->Reset();
		break;
	}
}


void CPlayScenceKeyHandler::KeyState(BYTE *states)
{
	CGame *game = CGame::GetInstance();
	CMario *mario = ((CPlayScene*)scence)->GetPlayer();
	// disable control key when Mario die 
	
	if (mario->GetState() == MARIO_STATE_DIE) return;
	else if (game->IsKeyDown(DIK_LSHIFT) && game->IsKeyDown(DIK_LEFT) && !game->IsKeyDown(DIK_RIGHT))
	{
		if (mario->previousstate == MARIO_STATE_RUNNING_RIGHT || mario->previousstate == MARIO_STATE_WALKING_RIGHT)
		{
			mario->changeDirection = true;
			mario->SetState(MARIO_STATE_CHANGEDIRECTION);
		}			
		else
		{
			mario->changeDirection = false;
			mario->SetState(MARIO_STATE_RUNNING_LEFT);
		}			
	}		
	else if (game->IsKeyDown(DIK_LSHIFT) && game->IsKeyDown(DIK_RIGHT)&&!game->IsKeyDown(DIK_LEFT))
	{
		if (mario->previousstate == MARIO_STATE_RUNNING_LEFT || mario->previousstate == MARIO_STATE_WALKING_LEFT)
		{
			mario->changeDirection = true;
			mario->SetState(MARIO_STATE_CHANGEDIRECTION);
		}			
		else
		{
			mario->changeDirection = false;
			mario->SetState(MARIO_STATE_RUNNING_RIGHT);
		}			
	}
		
	else if (game->IsKeyDown(DIK_RIGHT) && !game->IsKeyDown(DIK_LEFT))
	{
		if (mario->previousstate == MARIO_STATE_RUNNING_LEFT || mario->previousstate == MARIO_STATE_WALKING_LEFT)
		{
			mario->changeDirection = true;
			mario->SetState(MARIO_STATE_CHANGEDIRECTION);
		}
		else
		{
			mario->changeDirection = false;
			mario->SetState(MARIO_STATE_WALKING_RIGHT);

		}
	}
	else if (game->IsKeyDown(DIK_LEFT)&& !game->IsKeyDown(DIK_RIGHT))
	{
		if (mario->previousstate == MARIO_STATE_RUNNING_RIGHT || mario->previousstate == MARIO_STATE_WALKING_RIGHT)
		{
			mario->changeDirection = true;
			mario->SetState(MARIO_STATE_CHANGEDIRECTION);
		}			
		else
		{
			mario->changeDirection = false;
			mario->SetState(MARIO_STATE_WALKING_LEFT);
		}
	}
	else
	{
		mario->changeDirection = false;
		mario->SetState(MARIO_STATE_IDLE);
	}		
	mario->previousstate = mario->GetState();	
}