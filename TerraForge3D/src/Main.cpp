#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include "../resource.h"
#include <Base.h>
#include <Base/EntryPoint.h>
#include <ImGuiConsole.h>
#include <AppStyles.h>
#include <ModelImporter.h>
#include <ExplorerControls.h>
#include <VersionInfo.h>
#include <Texture2D.h>
#include <TextureSettings.h>
#include <ViewportFramebuffer.h>
#include <Modules/ModuleManager.h>
#include <AppShaderEditor.h>
#include <UIFontManager.h>
#include <ProjectData.h>
#include <OSLiscenses.h>
#include <ExportTexture.h>
#include <FrameBuffer.h>
#include <FiltersManager.h>
#include <FoliagePlacement.h>
#include <SkySettings.h>
#include <SupportersTribute.h>
#include <MeshNodeEditor.h>
#include <ExportManager.h>
#include <TextureStore.h>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <string>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_relational.hpp>
#include <glm/ext/vector_relational.hpp>
#include <glm/ext/scalar_relational.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <SimplexNoise.h>
#include <FastNoiseLite.h>
#include <thread>
#include <experimental/filesystem>
#include <time.h>
#include <string.h>
#include <mutex>
#include <memory>
#include <chrono>
#include <json.hpp>
#include <zip.h>
#include <sys/stat.h>
#include <dirent/dirent.h>
#include <atomic>

#ifdef TERR3D_WIN32
#include <windows.h>
#endif


#include <Utils.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "NoiseLayers/LayeredNoiseManager.h"


#include <AppStructs.h>

static Model terrain("Terrain");
static Model sea("Sea");
static Model grid("Grid");
static Model screenQuad("Screen Quad");

static Model* customModel,* customModelCopy;

static FrameBuffer* reflectionfbo, * textureFBO;

static Application* myApp;
static Shader* shd, * meshNormalsShader, * wireframeShader, * waterShader, * textureBakeShader, * foliageShader;
static std::shared_ptr<Shader> postProcessingShader;
static Camera camera;
static Camera postCamera;
static Stats s_Stats;
static ActiveWindows activeWindows;
static LayeredNoiseManager* noiseGen;
static const char* noiseTypes[] = { "Simplex Perlin", "Random", "Cellular" };
static const char* baseShapes[] = { "Plane", "Icosphere"};
static char* currentBaseShape = (char*)noiseTypes[0];
static ModuleManager* moduleManager;

static glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
static glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
static glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
static glm::vec3 LightPosition = glm::vec3(0.0f);
static float LightColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static float SeaColor[4] = { 0.4f, 0.4f, 1.0f, 1.0f };
static float CameraPosition[3];
static float CameraRotation[3];
static Vec2 prevMousePos;
static char b1[4096], b2[4096];

static bool isUsingBase = true;
static bool skyboxEnabled = false;
static bool vSync = true;
static bool autoUpdate = false;
static bool flattenBase = false;
static bool button1, button2, button3;
static std::atomic<bool> noiseBased = true;
static bool wireFrameMode = false;
static bool showSea = false;
static bool absolute = false;
static bool square = false;
static bool reqTexRfrsh = false;
static bool autoSave = false;
static bool isExploreMode = false;
static bool isIExploreMode = false;
static bool showFoliage = true;
static bool isTextureBake = false;
static std::atomic<bool> isRemeshing = false;
static std::atomic<bool> isRuinning = true;
static bool isPostProcess = false;

static Texture2D* diffuse, * normal, * gridTex, * waterDudvMap, * waterNormal;


static uint32_t vao, vbo, ebo;

static float noiseStrength = 1.0f;
static int resolution = 256;
static float mouseSpeed = 25;
static float scrollSpeed = 0.5f;
static float mouseScrollAmount = 0;
static float seaLevel = 0.0f;
static float scale = 1.0f;
static float textureScale = 1.0f;
static float textureScaleO = 1.0f;
static float viewportMousePosX = 0;
static float viewportMousePosY = 0;
static int numberOfNoiseTypes = 3;
static float seaAlpha = 0.5f;
static float seaDistortionScale = 1.0f;
static float seaDistortionStength = 0.02f;
static float seaReflectivity = 0.7;
static float seaWaveSpeed = 0.01f;
static int secondCounter = 0;

static nlohmann::json appData;

static std::string successMessage = "";
static std::string errorMessage = "";
static std::string savePath = "";
static std::string DuDvMapID = "";
static std::string waterNormalMap = "";
static std::string currentBaseModelName = "";

static float nLOffsetX, nLOffsetY, nLOffsetZ;

std::shared_ptr<FrameBuffer> postProcessFBO;

static float cloudsBoxMin[3] = { 0, 0, 0 };
static float cloudsBoxMax[3] = {1, 1, 1};


static void ToggleSystemConsole() {
	static bool state = false;
	state = !state;
#ifdef TERR3D_WIN32
	ShowWindow(GetConsoleWindow(), state ? SW_SHOW : SW_HIDE);
#else 
	std::cout << "Toogle Console Not Supported on Linux!" < std::endl;
#endif
}



void OnAppClose(int x, int y)
{
	Log("Close App");
	myApp->Close();
}


static void ShowStats()
{
	ImGui::Begin("Statistics", &activeWindows.statsWindow);

	// Frame Rate
	{
		if (s_Stats.deltaTime == 0)
			s_Stats.deltaTime = 1;
		memset(b1, 0, 100);
		memset(b2, 0, 100);
		_itoa_s((int)s_Stats.frameRate, b2, 10);
		strcat_s(b1, "FPS         : ");
		strcat_s(b1, b2);
		ImGui::Text(b1);

		memset(b1, 0, 100);
		memset(b2, 0, 100);
		_gcvt_s(b2, s_Stats.deltaTime * 1000, 6);
		strcat_s(b1, "Frame Time  : ");
		strcat_s(b1, b2);
		strcat_s(b1, "ms");
		ImGui::Text(b1);

		memset(b1, 0, 100);
		memset(b2, 0, 100);
		_itoa_s(s_Stats.triangles, b2, 10);
		strcat_s(b1, "Triangles  : ");
		strcat_s(b1, b2);
		ImGui::Text(b1);

		memset(b1, 0, 100);
		memset(b2, 0, 100);
		_itoa_s(s_Stats.vertCount, b2, 10);
		strcat_s(b1, "Vertices  : ");
		strcat_s(b1, b2);
		ImGui::Text(b1);

		ImGui::Text(("Mesh Generation Time : " + std::to_string(s_Stats.meshGenerationTime)).c_str());
	}

	ImGui::End();
}

static void ShowGeneralControls()
{
	ImGui::Begin("General Controls");

	{
		ImGui::Checkbox("VSync ", &vSync);
	}

	ImGui::DragFloat("Mouse Speed", &mouseSpeed);
	ImGui::DragFloat("Zoom Speed", &scrollSpeed);

	if (ImGui::Button("Exit")) {
		exit(0);
	}

	ImGui::End();
}

static void ShowCameraControls() {
	ImGui::Begin("Camera Controls");

	ImGui::Text("Camera Position");
	ImGui::DragFloat3("##cameraPosition", CameraPosition, 0.1f);
	ImGui::Separator();
	ImGui::Separator();
	ImGui::Text("Camera Rotation");
	ImGui::DragFloat3("##cameraRotation", CameraRotation, 10);
	ImGui::Separator();
	ImGui::Separator();
	ImGui::Text("Projection Settings");
	ImGui::Separator();
	ImGui::DragFloat("FOV", &(camera.fov), 0.01f);
	ImGui::DragFloat("Aspect Ratio", &(camera.aspect), 0.01f);
	ImGui::DragFloat("Near Clipping", &(camera.near), 0.01f);
	ImGui::DragFloat("Far Clipping", &(camera.far), 0.01f);

	ImGui::End();
}

static void ShowLightingControls() {
	ImGui::Begin("Sun Controls");

	ImGui::Text("Sun Position");
	ImGui::DragFloat3("##lightPosition", &LightPosition[0], 0.1f);
	ImGui::Separator();
	ImGui::Separator();
	ImGui::Text("Sun Light Color");
	ImGui::ColorEdit3("##lightColor", LightColor);

	ImGui::End();
}

static void DeleteBuffers(GLuint a, GLuint b, GLuint c) {
	glDeleteBuffers(1, &a);
	glDeleteBuffers(1, &b);
	glDeleteBuffers(1, &c);
}

static void ResetShader() {
	bool res = false;
	if (shd)
		delete shd;
	if (textureBakeShader)
		delete textureBakeShader;
	if(foliageShader)
		delete foliageShader;
	if (!wireframeShader)
		wireframeShader = new Shader(GetDefaultVertexShaderSource(), GetDefaultFragmentShaderSource(), GetWireframeGeometryShaderSource());
	if (!waterShader)
		waterShader = new Shader(ReadShaderSourceFile(GetExecutableDir() + "\\Data\\shaders\\water\\vert.glsl", &res), ReadShaderSourceFile(GetExecutableDir() + "\\Data\\shaders\\water\\frag.glsl", &res), ReadShaderSourceFile(GetExecutableDir() + "\\Data\\shaders\\water\\geom.glsl", &res));
	textureBakeShader = new Shader(ReadShaderSourceFile(GetExecutableDir() + "\\Data\\shaders\\texture_bake\\vert.glsl", &res), ReadShaderSourceFile(GetExecutableDir() + "\\Data\\shaders\\texture_bake\\frag.glsl", &res), ReadShaderSourceFile(GetExecutableDir() + "\\Data\\shaders\\texture_bake\\geom.glsl", &res));
	shd = new Shader(GetVertexShaderSource(), GetFragmentShaderSource(), GetGeometryShaderSource());
	foliageShader = new Shader(ReadShaderSourceFile(GetExecutableDir() + "\\Data\\shaders\\foliage\\vert.glsl", &res), ReadShaderSourceFile(GetExecutableDir() + "\\Data\\shaders\\foliage\\frag.glsl", &res), ReadShaderSourceFile(GetExecutableDir() + "\\Data\\shaders\\foliage\\geom.glsl", &res));
}

static void FillMeshData() {
	auto timeBegin = std::chrono::high_resolution_clock::now();
	if (isUsingBase) {
		s_Stats.vertCount = terrain.mesh->vertexCount;

		if (terrain.mesh->res != resolution || terrain.mesh->sc != scale || textureScale != textureScaleO) {

			terrain.mesh->GeneratePlane(resolution, scale, textureScale);
			s_Stats.triangles = terrain.mesh->indexCount / 3;
			s_Stats.vertCount = terrain.mesh->vertexCount;
			textureScaleO = textureScale;
		}
		

		for (int y = 0; y < resolution; y++)
		{
			for (int x = 0; x < resolution; x++)
			{
				if (noiseBased)
				{
					float elev = noiseGen->Evaluate(x, y, 0);
					for (NoiseLayerModule* mod : moduleManager->nlModules)
					{
						if (mod->active)
						{
							elev += mod->Evaluate(x, y, 0);
						}
					}
					terrain.mesh->SetElevation(elev, x, y);
				}
				else {
					float pos[3] = { (float)x, (float)y, 0.0f };
					float texCoord[2] = { (float)x / (resolution - 1), (float)y / (resolution - 1) };
					float minPos[3] = {0, 0, 0};
					float maxPos[3] = {256, 256, 0};
					terrain.mesh->SetElevation(EvaluateMeshNodeEditor(NodeInputParam(pos, texCoord, minPos, maxPos)).value, x, y);
				}
			}
		}
		terrain.mesh->RecalculateNormals();	
	}
	else{
		s_Stats.vertCount = customModel->mesh->vertexCount;
		for(int i=0;i<customModel->mesh->vertexCount;i++)
		{
			Vert tmp = customModelCopy->mesh->vert[i];
			float x = tmp.position.x;
			float y = tmp.position.y;
			float z = tmp.position.z;
			float elev = 0.0f;
			if (noiseBased)
			{
				elev = noiseGen->Evaluate(x, y, z);
				for (NoiseLayerModule* mod : moduleManager->nlModules)
				{
					if (mod->active)
					{
						elev += mod->Evaluate(x, y, z);
					}
				}
			}
			else {
				float pos[3] = { x, y, z };
				float texCoord[2] = { tmp.texCoord.x, tmp.texCoord.y };
				float minPos[3] = {0, 0, 0};
				float maxPos[3] = {-1, -1, -1};
				elev =  EvaluateMeshNodeEditor(NodeInputParam(pos, texCoord, minPos, maxPos)).value;
				
			}
			tmp.position *= scale;
			tmp.position += elev * tmp.normal;
			customModel->mesh->vert[i].extras1.x = elev;
			customModel->mesh->vert[i].position = tmp.position;			
		}
		customModel->mesh->RecalculateNormals();
		
	}	
	auto timeEnd = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> timeElapsed = timeEnd - timeBegin;
	s_Stats.meshGenerationTime = timeElapsed.count();
	isRemeshing = false;
}

static void RegenerateMesh() {

	if (isRemeshing)
		return;

	if(isUsingBase)
		terrain.UploadToGPU();
	else
		customModel->UploadToGPU();

	noiseGen->UpdateLayers();

	isRemeshing = true;

	std::thread worker(FillMeshData);
	worker.detach();


}

static void GenerateMesh()
{
	ResetShader();

	FillMeshData();

	terrain.SetupMeshOnGPU();
	sea.SetupMeshOnGPU();
	sea.mesh->GeneratePlane(256, 120);
	sea.mesh->RecalculateNormals();
	sea.UploadToGPU();

	grid.SetupMeshOnGPU();
	grid.mesh->GeneratePlane(50, 120);
	grid.mesh->RecalculateNormals();
	grid.UploadToGPU();

	screenQuad.SetupMeshOnGPU();
	screenQuad.mesh->GenerateScreenQuad();
	screenQuad.mesh->RecalculateNormals();
	screenQuad.UploadToGPU();
}

static void DoTheRederThing(float deltaTime, bool renderWater = false, bool bakeTexture = false) {

	static float time;
	time += deltaTime;
	camera.UpdateCamera(CameraPosition, CameraRotation);
	Shader* shader;
	// Texture Bake
	if (isTextureBake)
	{
		Camera cam;
		float CameraP[3] = { 0.0f, 0.0f, CameraPosition[2] };
		float CameraR[3] = { 5185.0f, 0.0f, 0.0f };
		cam.far = 1000;
		cam.aspect = 1;
		cam.UpdateCamera(CameraP, CameraR);
		shader = textureBakeShader;
		shader->Bind();
		shader->SetTime(&time);
		shader->SetMPV(cam.pv);
		shader->SetUniformMat4("_Model", terrain.modelMatrix);
		shader->SetLightCol(LightColor);
		shader->SetLightPos(LightPosition);
		float tmp[3];
		tmp[0] = 1024;
		tmp[1] = 1024;
		tmp[2] = 1;
		shader->SetUniform3f("_Resolution", tmp);
		shader->SetUniform3f("_CameraPos", CameraPosition);
		shader->SetUniformf("_SeaLevel", seaLevel);
		shader->SetUniformf("_CameraNear", camera.near);
		shader->SetUniformf("_CameraFar", camera.far);
		UpdateDiffuseTexturesUBO(shader->GetNativeShader(), "_DiffuseTextures");
		terrain.Render();
	}
	else {
		if (skyboxEnabled)
			RenderSky(camera.view, camera.pers);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		if (wireFrameMode)
			shader = wireframeShader;
		else
			shader = shd;
		shader->Bind();
		shader->SetTime(&time);
		shader->SetMPV(camera.pv);
		shader->SetUniformMat4("_Model", terrain.modelMatrix);
		shader->SetLightCol(LightColor);
		shader->SetLightPos(LightPosition);
		float tmp[3];
		tmp[0] = viewportMousePosX;
		tmp[1] = viewportMousePosY;
		tmp[2] = ImGui::GetIO().MouseDown[0];
		shader->SetUniform3f("_MousePos", tmp);
		tmp[0] = 800;
		tmp[1] = 600;
		tmp[2] = 1;
		shader->SetUniform3f("_Resolution", tmp);
		shader->SetUniform3f("_CameraPos", CameraPosition);
		shader->SetUniformf("_SeaLevel", seaLevel);
		shader->SetUniformf("_CameraNear", camera.near);
		shader->SetUniformf("_CameraFar", camera.far);
		UpdateDiffuseTexturesUBO(shader->GetNativeShader(), "_DiffuseTextures");
		if(isUsingBase)
			terrain.Render();
		else
			customModel->Render();


		if (showFoliage)
		{
			shader = foliageShader;
			shader->Bind();
		shader->SetTime(&time);
		shader->SetMPV(camera.pv);
		shader->SetUniformMat4("_Model", terrain.modelMatrix);
		shader->SetLightCol(LightColor);
		shader->SetLightPos(LightPosition);
		float tmp[3];
		tmp[0] = viewportMousePosX;
		tmp[1] = viewportMousePosY;
		tmp[2] = ImGui::GetIO().MouseDown[0];
		shader->SetUniform3f("_MousePos", tmp);
		tmp[0] = 800;
		tmp[1] = 600;
		tmp[2] = 1;
		shader->SetUniform3f("_Resolution", tmp);
		shader->SetUniform3f("_CameraPos", CameraPosition);
		shader->SetUniformf("_SeaLevel", seaLevel);
		shader->SetUniformf("_CameraNear", camera.near);
		shader->SetUniformf("_CameraFar", camera.far);
			RenderFoliage(shader, camera);
		}



		// For Future
		//gridTex->Bind(5);
		//grid->Render();


		if (showSea && renderWater) {
			waterShader->Bind();
			waterShader->SetTime(&time);
			waterShader->SetUniformf("_SeaAlpha", seaAlpha);
			waterShader->SetUniformf("_SeaDistScale", seaDistortionScale);
			waterShader->SetUniformf("_SeaDistStrength", seaDistortionStength);
			waterShader->SetUniformf("_SeaReflectivity", seaReflectivity);
			waterShader->SetUniformf("_SeaLevel", seaLevel);
			waterShader->SetUniformf("_SeaWaveSpeed", seaWaveSpeed);
			glActiveTexture(7);
			glBindTexture(GL_TEXTURE_2D, reflectionfbo->GetColorTexture());
			waterShader->SetUniformi("_ReflectionTexture", 7);
			if (waterDudvMap)
				waterDudvMap->Bind(8);
			waterShader->SetUniformi("_DuDvMap", 8);
			if (waterNormal)
				waterNormal->Bind(9);
			waterShader->SetUniformi("_NormalMap", 9);
			sea.position.y = seaLevel;
			sea.Update();
			glm::mat4 tmp = glm::translate(camera.pv, glm::vec3(0, seaLevel, 0));
			waterShader->SetUniform3f("_SeaColor", SeaColor);
			waterShader->SetMPV(tmp);
			waterShader->SetLightCol(LightColor);
			waterShader->SetLightPos(LightPosition);
			sea.Render();
		}
	}


	 
}

void PostProcess(float deltatime)
{
	float pos[3] = { 0, 0, 1.8 };
	float rot[3] = {0, 0, 0};
	postCamera.aspect = 1;
	postCamera.UpdateCamera(pos, rot);

	float viewDir[3] = { glm::value_ptr(camera.pv)[2], glm::value_ptr(camera.pv)[6], glm::value_ptr(camera.pv)[10] };

	postProcessingShader->Bind(); 

	postProcessingShader->SetUniform3f("_CameraPosition", CameraPosition);
	postProcessingShader->SetUniform3f("_ViewDir", viewDir);

	postProcessingShader->SetUniform3f("cloudsBoxBoundsMin", cloudsBoxMin);
	postProcessingShader->SetUniform3f("cloudsBoxBoundsMax", cloudsBoxMax);
	 
	postProcessingShader->SetMPV(postCamera.pv);

	glActiveTexture(GL_TEXTURE5);	
	glBindTexture(GL_TEXTURE_2D, GetViewportFramebufferColorTextureId());
	postProcessingShader->SetUniformi("_ColorTexture", 5);

	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, GetViewportFramebufferDepthTextureId()); 
	postProcessingShader->SetUniformi("_DepthTexture", 6);

	screenQuad.Render();
} 
 
static void ChangeCustomModel(std::string mdstr = ShowOpenFileDialog("*.obj"))
{
	if(!isUsingBase)
	{
		while(isRemeshing);
		delete customModel;
		delete customModelCopy;
	}
	currentBaseModelName = mdstr;
	if(currentBaseModelName.size() > 3)
	{
		isUsingBase = false;	
		customModel = LoadModel(currentBaseModelName);
		customModelCopy = LoadModel(currentBaseModelName); // Will Be Replaced with something efficient
	}
}

static void ShowChooseBaseModelPopup()
{
	// TEMP
	static std::shared_ptr<Texture2D> plane = std::make_shared<Texture2D>(GetExecutableDir() + "\\Data\\textures\\ui_thumbs\\plane.png", false, false);
	static std::shared_ptr<Texture2D> sphere = std::make_shared<Texture2D>(GetExecutableDir() + "\\Data\\textures\\ui_thumbs\\sphere.png", false, false);
	static std::shared_ptr<Texture2D> cube = std::make_shared<Texture2D>(GetExecutableDir() + "\\Data\\textures\\ui_thumbs\\cube.png", false, false);
	static std::shared_ptr<Texture2D> cone = std::make_shared<Texture2D>(GetExecutableDir() + "\\Data\\textures\\ui_thumbs\\cone.png", false, false);
	static std::shared_ptr<Texture2D> cyllinder = std::make_shared<Texture2D>(GetExecutableDir() + "\\Data\\textures\\ui_thumbs\\cylinder.png", false, false);
	static std::shared_ptr<Texture2D> torus = std::make_shared<Texture2D>(GetExecutableDir() + "\\Data\\textures\\ui_thumbs\\torus.png", false, false);
	// TEMP

	if (ImGui::BeginPopup("Choose Base Model"))
	{
		if (ImGui::Button("Open From File")) 
		{
			ChangeCustomModel();
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::Button("Close"))
			ImGui::CloseCurrentPopup();

		ImGui::BeginChild("Choose Base Model##Child", ImVec2(650, 450));

		if (ImGui::ImageButton((ImTextureID)plane->GetRendererID(), ImVec2(200, 200)) )
		{
			while (isRemeshing);
			delete customModel;
			delete customModelCopy;
			isUsingBase = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::ImageButton((ImTextureID)sphere->GetRendererID(), ImVec2(200, 200)))
		{
			ChangeCustomModel(GetExecutableDir() + "\\Data\\models\\sphere.obj");
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::ImageButton((ImTextureID)cube->GetRendererID(), ImVec2(200, 200)))
		{
			ChangeCustomModel(GetExecutableDir() + "\\Data\\models\\cube.obj");
			ImGui::CloseCurrentPopup();
		}


		if (ImGui::ImageButton((ImTextureID)torus->GetRendererID(), ImVec2(200, 200)))
		{
			ChangeCustomModel(GetExecutableDir() + "\\Data\\models\\torus.obj");
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::ImageButton((ImTextureID)cone->GetRendererID(), ImVec2(200, 200)))
		{
			ChangeCustomModel(GetExecutableDir() + "\\Data\\models\\cone.obj");
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::ImageButton((ImTextureID)cyllinder->GetRendererID(), ImVec2(200, 200)))
		{
			ChangeCustomModel(GetExecutableDir() + "\\Data\\models\\cylinder.obj");
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndChild();

		ImGui::EndPopup();
	}
}

static void ShowTerrainControls()
{
	static bool exp = false;
	ImGui::Begin("Dashboard");

	ShowChooseBaseModelPopup();
	
	if(!isUsingBase)
	{
		ImGui::Text(("Current Base Model : " + currentBaseModelName).c_str());
		if(ImGui::Button("Change Current Base"))
		{
			ImGui::OpenPopup("Choose Base Model");
		}
		if(ImGui::Button("Switch to Plane"))
		{
			while(isRemeshing);
			delete customModel;
			delete customModelCopy;
			isUsingBase = true;
		}
	}
	else
	{
		ImGui::Text("Current Base Model : Default Plane");
		if (ImGui::Button("Change Current Base"))
		{
			ImGui::OpenPopup("Choose Base Model");
		}
	}


	ImGui::DragInt("Mesh Resolution", &resolution, 1, 2, 8192);
	ImGui::DragFloat("Mesh Scale", &scale, 0.1f, 1.0f, 5000.0f);
	ImGui::NewLine();

	// TMP
	ImGui::DragFloat3("Clouds Box Max", cloudsBoxMax, 0.1f);
	ImGui::DragFloat3("Clouds Box Min", cloudsBoxMin, 0.1f);
	// TMP

	ImGui::NewLine();
	ImGui::Checkbox("Post Processing", &isPostProcess);
	ImGui::Checkbox("Auto Save", &autoSave);
	ImGui::Checkbox("Auto Update", &autoUpdate);
	ImGui::Checkbox("Wireframe Mode", &wireFrameMode);
	ImGui::Checkbox("Show Skybox", &skyboxEnabled);
	ImGui::Checkbox("Show Foliage", &showFoliage);
	ImGui::Checkbox("Infinite Explorer Mode", &isIExploreMode);
	ImGui::Checkbox("Explorer Mode", &isExploreMode);
	ImGui::Checkbox("Texture Bake Mode", &isTextureBake);
	ImGui::NewLine();
	if (ImGui::Button("Update Mesh"))
		RegenerateMesh();

	if (ImGui::Button("Recalculate Normals")) {
		while (isRemeshing);
		terrain.mesh->RecalculateNormals();
		terrain.UploadToGPU();
	}

	if (ImGui::Button("Refresh Shaders")) {
		ResetShader();
	}

	if (ImGui::Button("Use Custom Shaders")) {
		activeWindows.shaderEditorWindow = true;
	}

	if (ImGui::Button("Texture Settings")) {
		activeWindows.texturEditorWindow = true;
	}

	if (ImGui::Button("Sea Settings")) {
		activeWindows.seaEditor = true;
	}
	if (ImGui::Button("Filter Settings")) {
		activeWindows.filtersManager = true;
	}

	if (ImGui::Button("Export Frame")) {
		if(isTextureBake)
			ExportTexture(textureFBO->GetRendererID() , ShowSaveFileDialog(".png"), 1024, 1024);
		else
			ExportTexture(GetViewportFramebufferId(), ShowSaveFileDialog(".png"), 800, 600);
	}
	ImGui::Separator();

	if (ImGui::Button("Change Mode##4584")) {
		noiseBased = !noiseBased;
	}

	if (noiseBased) {
		ImGui::Text("Using Layer Based Workflow");
	}
	else {
		ImGui::Text("Using Node Based Workflow");
	}

	ImGui::Separator();

	ImGui::NewLine();

	if(moduleManager->uiModules.size() > 0)
		ImGui::Text("UI Modules");
	int i = 0;
	for (UIModule* mod : moduleManager->uiModules)
		ImGui::Checkbox(MAKE_IMGUI_LABEL(i++, mod->windowName), &mod->active);

	ImGui::Separator();

	ImGui::NewLine();

	if (moduleManager->nlModules.size() > 0)
		ImGui::Text("Noise Layer Modules");
	i = 0;
	for (NoiseLayerModule* mod : moduleManager->nlModules)
		ImGui::Checkbox(MAKE_IMGUI_LABEL(i++, mod->name), &mod->active);
	ImGui::End();
}

static void ShowNoiseSettings() {
	ImGui::Begin("Noise Settings");

	noiseGen->Render();

	ImGui::End();
}

static void MouseMoveCallback(float x, float y) {
	float deltaX = x - prevMousePos.x;
	float deltaY = y - prevMousePos.y;

	if (button2) {


		if (deltaX > 200) {
			deltaX = 0;
		}

		if (deltaY > 200) {
			deltaY = 0;
		}

		CameraRotation[0] += deltaX * mouseSpeed;
		CameraRotation[1] += deltaY * mouseSpeed;
	}


	prevMousePos.x = x;
	prevMousePos.y = y;

}

static void MouseScrollCallback(float amount) {
	mouseScrollAmount = amount;
	glm::vec3 pPos = glm::vec3(CameraPosition[0], CameraPosition[1], CameraPosition[2]);
	pPos += front * amount * scrollSpeed;
	CameraPosition[0] = pPos.x;
	CameraPosition[1] = pPos.y;
	CameraPosition[2] = pPos.z;
}


static void ShowMainScene() {
	auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
	auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
	auto viewportOffset = ImGui::GetWindowPos();

	ImGui::Begin("Viewport");
	{
		ImGui::BeginChild("MainRender");
		if (ImGui::IsWindowHovered()) {
			ImGuiIO io = ImGui::GetIO();
			MouseMoveCallback(io.MousePos.x, io.MousePos.y);
			MouseScrollCallback(io.MouseWheel);
			button1 = io.MouseDown[0];
			button2 = io.MouseDown[2];
			button3 = io.MouseDown[1];
			if (ImGui::GetIO().MouseDown[1]) {
				CameraPosition[0] += -io.MouseDelta.x * 0.005f * glm::distance(glm::vec3(0.0f), glm::vec3(CameraPosition[0], CameraPosition[1], CameraPosition[2]));
				CameraPosition[1] += io.MouseDelta.y * 0.005f * glm::distance(glm::vec3(0.0f), glm::vec3(CameraPosition[0], CameraPosition[1], CameraPosition[2]));
			}
			viewportMousePosX = ImGui::GetIO().MousePos.x - viewportOffset.x;
			viewportMousePosY = ImGui::GetIO().MousePos.y - viewportOffset.y;

		}
		ImVec2 wsize = ImGui::GetWindowSize();
		if (isTextureBake)
			ImGui::Image((ImTextureID)textureFBO->GetColorTexture(), wsize, ImVec2(0, 1), ImVec2(1, 0));
		else if (isPostProcess)
			ImGui::Image((ImTextureID)postProcessFBO->GetColorTexture(), wsize, ImVec2(0, 1), ImVec2(1, 0));
		else
			ImGui::Image((ImTextureID)GetViewportFramebufferColorTextureId(), wsize, ImVec2(0, 1), ImVec2(1, 0));
		ImGui::EndChild();
	}
	ImGui::End();
}

static void SaveFile(std::string file = ShowSaveFileDialog()) {
	if (file.size() == 0)
		return;
	if (file.find(".terr3d") == std::string::npos)
		file += ".terr3d";

	if (file.find("autosave.terr3d") == std::string::npos)
		savePath = file;

	nlohmann::json data;
	data["type"] = "SAVEFILE";
	data["serializerVersion"] = TERR3D_SERIALIZER_VERSION;
	data["versionHash"] = MD5File(GetExecutablePath()).ToString();
	data["name"] = "TerraForge3D v2.0.0";
	data["EnodeEditor"] = GetMeshNodeEditorSaveData();
	data["styleData"] = GetStyleData();
	data["appData"] = appData;
	data["imguiData"] = std::string(ImGui::SaveIniSettingsToMemory());
	data["texLayers"] = SaveTextureLayerData();

	
	data["noiseLayers"] = noiseGen->Save();
	nlohmann::json tmp;
	tmp["autoUpdate"] = autoUpdate;
	tmp["square"] = square;
	tmp["nLOffsetX"] = nLOffsetX;
	tmp["nLOffsetY"] = nLOffsetY;
	tmp["nLOffsetZ"] = nLOffsetZ;
	tmp["absolute"] = absolute;
	tmp["flattenBase"] = flattenBase;
	tmp["noiseBased"] = noiseBased ? true : false;
	tmp["wireFrameMode"] = wireFrameMode;
	tmp["skyboxEnabled"] = skyboxEnabled;
	tmp["showSea"] = showSea;
	tmp["vSync"] = vSync;
	tmp["mouseSpeed"] = mouseSpeed;
	tmp["scrollSpeed"] = scrollSpeed;
	tmp["mouseScrollAmount"] = mouseScrollAmount;
	tmp["scale"] = scale;
	tmp["textureScale"] = textureScale;
	tmp["textureScaleO"] = textureScaleO;
	tmp["seaLevel"] = seaLevel;
	tmp["seaAlpha"] = seaAlpha;
	tmp["autoSave"] = autoSave;
	tmp["seaDistortionStength"] = seaDistortionStength;
	tmp["seaDistortionScale"] = seaDistortionScale;
	tmp["seaReflectivity"] = seaReflectivity;
	tmp["projectID"] = GetProjectId();
	tmp["seaWaveSpeed"] = seaWaveSpeed;
	tmp["projectDatabase"] = GetProjectDatabase();
	tmp["projectDatabaseJs"] = nlohmann::json::parse(GetProjectDatabase());
	tmp["dudvmapID"] = DuDvMapID;
	tmp["waterNormalMap"] = waterNormalMap;

	SaveProjectDatabase();

	tmp["cameraPosX"] = CameraPosition[0];
	tmp["cameraPosY"] = CameraPosition[2];
	tmp["cameraPosZ"] = CameraPosition[1];

	tmp["cameraRotX"] = CameraRotation[0];
	tmp["cameraRotY"] = CameraRotation[1];
	tmp["cameraRotZ"] = CameraRotation[2];

	tmp["lightPosX"] = LightPosition[0];
	tmp["lightPosY"] = LightPosition[2];
	tmp["lightPosZ"] = LightPosition[1];

	if (diffuse) {
		bool isOk = true;
		static std::string uid;
		static std::string oPath = "";
		try {
			std::string path = GetProjectAsset(uid);
			if (path.size() > 0 && diffuse->GetPath() == oPath) {
				isOk = true;
			}
			else
				isOk = false;
		}
		catch (...) {
			isOk = false;
		}
		if (!isOk) {
			uid = GenerateId(32);
			oPath = diffuse->GetPath();
			if (!PathExist(GetProjectResourcePath() + "\\textures"))
				MkDir(GetProjectResourcePath() + "\\textures");
			CopyFileData(diffuse->GetPath(), GetProjectResourcePath() + "\\textures\\" + uid);
			RegisterProjectAsset(uid, "textures\\" + uid);
			Log("Saved Texture with ID : " + uid);
		}
		tmp["diffuseTexId"] = uid;
	}

	tmp["isUsingBase"] = isUsingBase;

	if(!isUsingBase)
	{
		std::string baseHash = MD5File(currentBaseModelName).ToString();
		std::string projectAsset = GetProjectAsset(baseHash); 
		if(projectAsset == "")
		{
			if (!PathExist(GetProjectResourcePath() + "\\models"))
				MkDir(GetProjectResourcePath() + "\\models");
			CopyFileData(currentBaseModelName, GetProjectResourcePath() + "\\models\\" + baseHash + currentBaseModelName.substr(currentBaseModelName.find_last_of(".")));
			RegisterProjectAsset(baseHash, "models\\" + baseHash + currentBaseModelName.substr(currentBaseModelName.find_last_of(".")));
		}
		tmp["baseid"] = baseHash;
	}

	data["generals"] = tmp;

	std::ofstream outfile;

	outfile.open(file);
	outfile << data.dump(4, ' ', false);
	outfile.close();

	outfile.open(GetProjectResourcePath() + "\\project.terr3d");
	outfile << data.dump(4, ' ', false);
	outfile.close();
}

static void OpenSaveFile(std::string file = ShowOpenFileDialog(".terr3d")) {

	// For Now it dows not do anything id any error has occured but in later versions this will be reported to user!


	if (file.size() == 0)
		return;
	if (file.find(".terr3d") == std::string::npos)
		file += ".terr3d";

	if (file.find("autosave.terr3d") == std::string::npos)
		savePath = file;

	bool flagRd = true;
	std::string sdata = ReadShaderSourceFile(file, &flagRd);
	if (!flagRd) {
		Log("Could not read file " + file);
		return;
	}
	if (sdata.size() == 0) {
		Log("Empty file.");
		return;
	}
	nlohmann::json data;
	try {
		data = nlohmann::json::parse(sdata);
	}
	catch (...) {
		Log("Failed to Parse file : " + file);
	}

	bool isSelfMade = true;

	try {
		if (data["versionHash"] != MD5File(GetExecutablePath()).ToString()) {
			Log("The file you are tryng to open was made with a different version of TerraForge3D!\nTrying to check Serializer compatibility.");
			isSelfMade = false;
		}
	}
	catch (...) {
		isSelfMade = false;
		Log("Failed to verify File version!");
		return;
	}

	if (!isSelfMade) {
		try {
			int sVersion = data["serializerVersion"];
			if (sVersion < TERR3D_MIN_SERIALIZER_VERSION) {
				Log("This file (" + file + ") cannot be opened as it was serialized using serializer V" + std::to_string(sVersion) + " but the minimum serializer version required is V" + std::to_string(TERR3D_MIN_SERIALIZER_VERSION));
				return;
			}
			if (sVersion > TERR3D_MAX_SERIALIZER_VERSION) {
				Log("This file (" + file + ") cannot be opened as it was serialized using serializer V" + std::to_string(sVersion) + " but the maximum serializer version supported is V" + std::to_string(TERR3D_MAX_SERIALIZER_VERSION));
				return;
			}
		}
		catch (...) {
			Log("Failed to verify Serializer!");
			return;
		}
	}


	if (data["type"] == "THEME") {
		LoadThemeFromStr(data.dump());
	}


	if (data["type"] != "SAVEFILE")
		return;

	LoadThemeFromStr(data["styleData"]);
	//data["imguiData"] = ImGui::SaveIniSettingsToMemory();
	appData = data["appData"];

	// This should be replaced with something better
	while (isRemeshing);
	noiseGen->Load(data["noiseLayers"]);
	nlohmann::json tmp = data["generals"];
	try {
		autoUpdate = tmp["autoUpdate"];

		nLOffsetY = tmp["nLOffsetY"];
		nLOffsetX = tmp["nLOffsetX"];
		nLOffsetZ = tmp["nLOffsetZ"];

		square = tmp["square"];
		absolute = tmp["absolute"];
		flattenBase = tmp["flattenBase"];
		noiseBased = tmp["noiseBased"];
		skyboxEnabled = tmp["skyboxEnabled"];
		vSync = tmp["vSync"];
		showSea = tmp["showSea"];
		wireFrameMode = tmp["wireFrameMode"];
		mouseSpeed = tmp["mouseSpeed"];
		scrollSpeed = tmp["scrollSpeed"];
		mouseScrollAmount = tmp["mouseScrollAmount"];
		scale = tmp["scale"];
		textureScale = tmp["textureScale"];
		textureScaleO = tmp["textureScaleO"];
		seaLevel = tmp["seaLevel"];
		seaAlpha = tmp["seaAlpha"];
		autoSave = tmp["autoSave"];
		seaReflectivity = tmp["seaReflectivity"];
		seaDistortionScale = tmp["seaDistortionScale"];
		seaWaveSpeed = tmp["seaWaveSpeed"];
		seaDistortionStength = tmp["seaDistortionStength"];
		DuDvMapID = tmp["dudvmapID"];
		waterNormalMap = tmp["waterNormalMap"];
		tmp["isUsingBase"] = isUsingBase;

	}
	catch (...) {}
	SetProjectId(tmp["projectID"]);
	try {
		SetProjectDatabase(tmp["projectDatabase"]);
	}
	catch (...) {}
	Log("Loaded Project ID : " + GetProjectId());

	LoadTextureLayerData(data["texLayers"]);
	SetMeshNodeEditorSaveData(data["EnodeEditor"]);


	CameraPosition[0] = tmp["cameraPosX"];
	CameraPosition[2] = tmp["cameraPosY"];
	CameraPosition[1] = tmp["cameraPosZ"];

	CameraRotation[0] = tmp["cameraRotX"];
	CameraRotation[1] = tmp["cameraRotY"];
	CameraRotation[2] = tmp["cameraRotZ"];

	LightPosition[0] = tmp["lightPosX"];
	LightPosition[2] = tmp["lightPosY"];
	LightPosition[1] = tmp["lightPosZ"];



	try{
		std::string baseHash = tmp["baseid"];
		std::string projectAsset = GetProjectAsset(baseHash); 
		if(projectAsset == "")
		{
			Log("Failed to load base model from save file!");
			Log("Loaded Base Model!");
			isUsingBase = true;
		}
		else
		{
			ChangeCustomModel(GetProjectResourcePath() + "\\" + projectAsset);
			isUsingBase = false;
		}
	}catch(...)
	{
		delete customModel;
		delete customModelCopy;
		isUsingBase = true;
		Log("Switching to Base Plane!");
	}

	if (DuDvMapID != "DEFAULT") {
		delete waterDudvMap;
		waterDudvMap = new Texture2D(GetProjectResourcePath() + "\\" + GetProjectAsset(DuDvMapID));
	}
	else {
		delete waterDudvMap;
		waterDudvMap = new Texture2D(GetExecutableDir() + "\\Data\\textures\\water_dudv.png");
	}

	if (waterNormalMap != "DEFAULT") {
		delete waterNormal;
		waterNormal = new Texture2D(GetProjectResourcePath() + "\\" + GetProjectAsset(waterNormalMap));
	}
	else {
		delete waterNormal;
		waterNormal = new Texture2D(GetExecutableDir() + "\\Data\\textures\\water_normal.png");
	}

	if (diffuse)
		delete diffuse;
	try {
		diffuse = new Texture2D(GetProjectResourcePath() + "\\" + GetProjectAsset(tmp["diffuseTexId"]));
	}

	catch (...) {
		Log("Cold not load texture from saved file.");
		diffuse = new Texture2D(GetExecutableDir() + "\\Data\\textures\\white.png");
	}

	// For Future
	// ImGui::LoadIniSettingsFromMemory(data["imguiData"].dump().c_str(), data["imguiData"].dump().size())
}

void zip_walk(struct zip_t* zip, const char* path, bool isFristLayer = true, std::string prevPath = "") {
	DIR* dir;
	struct dirent* entry;
	char fullpath[MAX_PATH];
	struct stat s;

	memset(fullpath, 0, MAX_PATH);
	dir = opendir(path);
	assert(dir);

	while ((entry = readdir(dir))) {
		// skip "." and ".."
		if (!strcmp(entry->d_name, ".\0") || !strcmp(entry->d_name, "..\0"))
			continue;

		snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
		stat(fullpath, &s);
		if (S_ISDIR(s.st_mode)) {
			zip_walk(zip, fullpath, false, prevPath + entry->d_name + "\\");
		}
		else {
			zip_entry_open(zip, (prevPath + entry->d_name).c_str());
			zip_entry_fwrite(zip, fullpath);
			zip_entry_close(zip);
		}
	}

	closedir(dir);
}

static void PackProject(std::string path = ShowSaveFileDialog()) {
	if (path.find(".terr3dpack") == std::string::npos)
		path = path + ".terr3dpack";
	if (savePath.size() > 0) {
		SaveFile(savePath);
	}
	else {
		if (!PathExist(GetExecutableDir() + "\\Data\\temp"))
			MkDir(GetExecutableDir() + "\\Data\\temp");
		std::string uid = GenerateId(64);
		SaveFile(GetExecutableDir() + "\\Data\\temp\\" + uid);
	}
	zip_t* packed = zip_open(path.c_str(), 9, 'w');
	zip_walk(packed, GetProjectResourcePath().c_str());
	zip_close(packed);
}

static void LoadPackedProject(std::string path = ShowOpenFileDialog()) {
	if (path.find(".terr3dpack") == std::string::npos)
		path = path + ".terr3dpack";
	std::string uid = GenerateId(64);
	MkDir(GetExecutableDir() + "\\Data\\temp\\pcache_" + uid);
	zip_extract(path.c_str(), (GetExecutableDir() + "\\Data\\temp\\pcache_" + uid).c_str(), [](const char* filename, void* arg) {return 1; }, (void*)0);

	if (FileExists(GetExecutableDir() + "\\Data\\temp\\pcache_" + uid + "\\project.terr3d", true)) {
		bool tmp = false;
		std::string sdata = ReadShaderSourceFile(GetExecutableDir() + "\\Data\\temp\\pcache_" + uid + "\\project.terr3d", &tmp);
		if (sdata.size() <= 0) {
			Log("Emppty Project File!");
			return;
		}
		nlohmann::json data;
		try {
			data = nlohmann::json::parse(sdata);
		}
		catch (...) {
			Log("Failed to Parse Project file!");
			return;
		}
		std::string projId = data["generals"]["projectID"];
		zip_extract(path.c_str(), (GetExecutableDir() + "\\Data\\cache\\project_data\\project_" + projId).c_str(), [](const char* filename, void* arg) {Log(std::string("Extracted ") + filename); return 1; }, (void*)0);
		std::string oriDir = path.substr(0, path.rfind("\\"));
		std::string oriName = path.substr(path.rfind("\\") + 1);
		CopyFileData((GetExecutableDir() + "\\Data\\cache\\project_data\\project_" + projId + "\\project.terr3d").c_str(), oriDir + "\\" + oriName + ".terr3d");
		OpenSaveFile(oriDir + "\\" + oriName + ".terr3d");
	}
	else {
		Log("Not a valid terr3dpack file!");
	}
}

static void ShowWindowMenuItem(const char* title, bool* val) {
	ImGui::Checkbox(title, val);
}

void OpenURL(std::string url)
{
#ifdef  TERR3D_WIN32
	std::string op = std::string("start ").append(url);
	system(op.c_str());
#else
	std::string op = std::string("xdg-open ").append(url);
	system(op.c_str());
#endif //  TERR3D_WIN32

}

static void ShowMenu() {
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open")) {
				OpenSaveFile();
			}

			if (ImGui::MenuItem("Save")) {
				SaveFile();
			}

			if (ImGui::MenuItem("Install Module")) {
				moduleManager->InstallModule(ShowOpenFileDialog("*.terr3dmodule"));
			}

			Model* modelToExport;
			if(isUsingBase)
				modelToExport = &terrain;
			else
				modelToExport = customModel;

			if (ImGui::BeginMenu("Export Mesh As")) {
				if (ImGui::MenuItem("Wavefont OBJ")) {
					ExportModelAssimp(modelToExport, "obj", ShowSaveFileDialog(".obj\0"), "obj");
				}

				if (ImGui::MenuItem("FBX")) {
					ExportModelAssimp(modelToExport, "fbx", ShowSaveFileDialog(".fbx\0"), "fbx");
				}

				if (ImGui::MenuItem("GLTF v2")) {
					ExportModelAssimp(modelToExport, "gltf2", ShowSaveFileDialog(".gltf\0"), "gltf");
				}

				if (ImGui::MenuItem("GLB v2")) {
					ExportModelAssimp(modelToExport, "glb2", ShowSaveFileDialog(".glb\0"), "glb");
				}

				if (ImGui::MenuItem("JSON")) {
					ExportModelAssimp(modelToExport, "json", ShowSaveFileDialog(".json\0"));
				}

				if (ImGui::MenuItem("STL")) {
					ExportModelAssimp(modelToExport, "stl", ShowSaveFileDialog(".stl\0"));
				}

				if (ImGui::MenuItem("PLY")) {
					ExportModelAssimp(modelToExport, "ply", ShowSaveFileDialog(".ply\0"));
				}

				if (ImGui::MenuItem("Collada")) {
					ExportModelAssimp(modelToExport, "collada", ShowSaveFileDialog(".dae\0"), "dae");
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Export Heightmap As")) {

				if (ImGui::MenuItem("PNG")) {
					if (ExportHeightmapPNG(terrain.mesh->Clone(), ShowSaveFileDialog(".png"))) {
						successMessage = "Sucessfully exported heightmap!";
						ImGui::BeginPopup("Success Messages");
					}
					else {
						errorMessage = "One Export is already in progress!";
						ImGui::BeginPopup("Error Messages");
					}
				}

				if (ImGui::MenuItem("JPG")) {
					if (ExportHeightmapJPG(terrain.mesh->Clone(), ShowSaveFileDialog(".jpg"))) {
						successMessage = "Sucessfully exported heightmap!";
						ImGui::BeginPopup("Success Messages");
					}
					else {
						errorMessage = "One Export is already in progress!";
						ImGui::BeginPopup("Error Messages");
					}
				}
				ImGui::EndMenu();
			}

			if (ImGui::MenuItem("Close")) {
				savePath = "";
			}

			if (ImGui::MenuItem("Pack Project")) {
				PackProject();
			}

			if (ImGui::MenuItem("Load Packed Project")) {
				LoadPackedProject();
			}

			if (ImGui::MenuItem("Load Auto Saved Project")) {
				OpenSaveFile(GetExecutableDir() + "\\Data\\cache\\autosave\\autosave.terr3d");
			}

			if (ImGui::MenuItem("Exit")) {
				exit(0);
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Options"))
		{
			if (ImGui::MenuItem("Toggle System Console")) {
				ToggleSystemConsole();
			}

			if (ImGui::MenuItem("Associate (.terr3d) File Type")) {
				AccocFileType();
			}

			if (ImGui::MenuItem("Copy Version Hash")) {
				char* output = new char[MD5File(GetExecutablePath()).ToString().size() + 1];
				strcpy(output, MD5File(GetExecutablePath()).ToString().c_str());
				const size_t len = strlen(output) + 1;
#ifdef TERR3D_WIN32
				HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
				memcpy(GlobalLock(hMem), output, len);
				GlobalUnlock(hMem);
				OpenClipboard(0);
				EmptyClipboard();
				SetClipboardData(CF_TEXT, hMem);
				CloseClipboard();
				delete[] output;
#else
				std::cout << "Version Hash : " << output << std::endl;
#endif
			}

			if (ImGui::BeginMenu("Themes")) {
				if (ImGui::MenuItem("Default")) {
					LoadDefaultStyle();
				}

				if (ImGui::MenuItem("Black & White")) {
					LoadBlackAndWhite();
				}

				if (ImGui::MenuItem("Cool Dark")) {
					LoadDarkCoolStyle();
				}

				if (ImGui::MenuItem("Light Orange")) {
					LoadLightOrngeStyle();
				}

				if (ImGui::MenuItem("Load Theme From File")) {
					LoadThemeFromFile(openfilename());
				}
				ImGui::EndMenu();
			}

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Widnows"))
		{
			ShowWindowMenuItem("Statistics", &activeWindows.statsWindow);

			ShowWindowMenuItem("Theme Editor", &activeWindows.styleEditor);

			ShowWindowMenuItem("Shader Editor", &activeWindows.shaderEditorWindow);

			ShowWindowMenuItem("Foliage Manager", &activeWindows.foliageManager);

			ShowWindowMenuItem("Elevation Node Editor", &activeWindows.elevationNodeEditorWindow);

			ShowWindowMenuItem("Texture Settings", &activeWindows.texturEditorWindow);

			ShowWindowMenuItem("Texture Store", &activeWindows.textureStore);

			ShowWindowMenuItem("Sea Settings", &activeWindows.seaEditor);

			ShowWindowMenuItem("Sky Settings", &activeWindows.skySettings);

			ShowWindowMenuItem("Filters Manager", &activeWindows.filtersManager);

			ShowWindowMenuItem("Module Manager", &activeWindows.modulesManager);

			ShowWindowMenuItem("Supporters", &activeWindows.supportersTribute);

			ShowWindowMenuItem("Open Source Liscenses", &activeWindows.osLisc);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help")) {
			if (ImGui::MenuItem("Major Contributers"))
				activeWindows.contribWindow = true;

			if (ImGui::MenuItem("Tutorial"))
				OpenURL("https://www.youtube.com/playlist?list=PLl3xhxX__M4A74aaTj8fvqApu7vo3cOiZ");

			if (ImGui::MenuItem("Social Handle"))
				OpenURL("https://twitter.com/jaysmito101");

			if (ImGui::MenuItem("Discord Server"))
				OpenURL("https://discord.gg/AcgRafSfyB");

			if (ImGui::MenuItem("GitHub Page"))
				OpenURL("https://github.com/Jaysmito101/TerraForge3D");

			if (ImGui::MenuItem("Documentation"))
				OpenURL("https://github.com/Jaysmito101/TerraForge3D/wiki");

			if (ImGui::MenuItem("Open Source Liscenses"))
				activeWindows.osLisc = !activeWindows.osLisc;

			ImGui::EndMenu();

		}
		ImGui::EndMainMenuBar();
	}
}

static void ShowErrorModal() {
	if (ImGui::BeginPopupModal("Error Messages")) {
		ImGui::TextColored(ImVec4(0.7f, 0.2f, 0.2f, 1.0f), errorMessage.c_str());
	}
}

static void ShowSuccessModal() {
	if (ImGui::BeginPopupModal("Success Messages")) {
		ImGui::TextColored(ImVec4(0.2f, 0.7f, 0.2f, 1.0f), successMessage.c_str());
	}
}

static void ShowSeaSettings() {
	ImGui::Begin("Sea Settings", &activeWindows.seaEditor);

	ImGui::Checkbox("Show Sea Level", &showSea);

	ImGui::Text("Sea Color");
	ImGui::ColorEdit3("##seaColor", SeaColor);
	ImGui::DragFloat("Alpha", &seaAlpha, 0.001f, 0, 1);
	ImGui::DragFloat("Reflectivity", &seaReflectivity, 0.001f, 0, 1);

	ImGui::Text("DuDv Map");
	ImGui::SameLine();
	if (ImGui::ImageButton((ImTextureID)waterDudvMap->GetRendererID(), ImVec2(50, 50))) {
		std::string fileName = ShowOpenFileDialog(".png");
		if (fileName.size() > 3) {
			delete waterDudvMap;

			std::string hash = MD5File(fileName).ToString();
			if (GetProjectAsset(hash).size() != 0) {
				DuDvMapID = hash;

			}
			else {
				CopyFileData(fileName, GetProjectResourcePath() + "\\textures\\" + hash);
				RegisterProjectAsset(hash, "textures\\" + hash);
				DuDvMapID = hash;
				waterDudvMap = new Texture2D(fileName);
			}
		}
	}


	ImGui::Text("Normal Map");
	ImGui::SameLine();
	if (ImGui::ImageButton((ImTextureID)waterNormal->GetRendererID(), ImVec2(50, 50))) {
		std::string fileName = ShowOpenFileDialog(".png");
		if (fileName.size() > 3) {
			delete waterNormal;

			std::string hash = MD5File(fileName).ToString();
			if (GetProjectAsset(hash).size() != 0) {
				waterNormalMap = hash;

			}
			else {
				CopyFileData(fileName, GetProjectResourcePath() + "\\textures\\" + hash);
				RegisterProjectAsset(hash, "textures\\" + hash);
				waterNormalMap = hash;
				waterNormal = new Texture2D(fileName);
			}
		}
	}

	ImGui::DragFloat("Distortion Strength", &seaDistortionStength, 0.001f);
	ImGui::DragFloat("Distortion Scale", &seaDistortionScale, 0.1f);
	ImGui::DragFloat("Wave Speed", &seaWaveSpeed, 0.01f);
	ImGui::DragFloat("Level", &seaLevel, 0.1f);


	ImGui::End();
}


static void ShowModuleManager() {
	ImGui::Begin("Module Manager", &activeWindows.modulesManager);
	moduleManager->Render();
	ImGui::End();
}

static void OnBeforeImGuiRender() {

	static bool dockspaceOpen = true;
	static bool opt_fullscreen_persistant = true;
	bool opt_fullscreen = opt_fullscreen_persistant;
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	if (opt_fullscreen)
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.1f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	}
	if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
		window_flags |= ImGuiWindowFlags_NoBackground;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
	ImGui::PopStyleVar();
	if (opt_fullscreen) {
		ImGui::PopStyleVar(2);
	}
	// DockSpace
	ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();
	float minWinSizeX = style.WindowMinSize.x;
	style.WindowMinSize.x = 370.0f;
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
	}

	style.WindowMinSize.x = minWinSizeX;

	ShowMenu();
}

static void OnImGuiRenderEnd() {
	ImGui::End();
}

static void SetUpIcon() {
#ifdef TERR3D_WIN32
	HWND hwnd = glfwGetWin32Window(myApp->GetWindow()->GetNativeWindow());
	HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	if (hIcon) {
		//Change both icons to the same icon handle.
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

		//This will ensure that the application icon gets changed too.
		SendMessage(GetWindow(hwnd, GW_OWNER), WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		SendMessage(GetWindow(hwnd, GW_OWNER), WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	}
#endif
}


class MyApp : public Application
{
public:
	virtual void OnPreload() override {
		SetTitle("TerraForge3D - Jaysmito Mukherjee");
		SetWindowConfigPath(GetExecutableDir() + "\\Data\\configs\\windowconfigs.terr3d");
		system((std::string("mkdir \"") + GetExecutableDir() + "\\Data\\cache\\autosave\"").c_str());
		SetupOSLiscences();
		Sleep(1000);
	}

	virtual void OnUpdate(float deltatime) override
	{
		if (!isRuinning)
			return;

		if (!isExploreMode) {
			if (reqTexRfrsh) {
				if (diffuse)
					delete diffuse;
				diffuse = GetCurrentDiffuseTexture();
				reqTexRfrsh = false;
			}

			// CTRL Shortcuts
			if ((glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_LEFT_CONTROL) || glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_RIGHT_CONTROL))) {

				// Open Shortcut
				if (glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_O)) {
					OpenSaveFile();
				}

				// Exit Shortcut
				if (glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_Q)) {
					exit(0);
				}

				// Export Shortcut
				if (glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_E)) {
					ExportOBJ(terrain.mesh->Clone(), ShowSaveFileDialog(".obj"));
				}

				// Save Shortcut
				if (glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_S)) {
					if (savePath.size() > 3) {
						Log("Saved to " + savePath);
						SaveFile(savePath);
					}
					else {
						SaveFile();
					}
				}

				// Close Shortcut
				if (glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_W)) {
					if (savePath.size() > 3) {
						Log("CLosed file " + savePath);
						savePath = "";
					}
					else {
						Log("Shutting Down");
						exit(0);
					}
				}

				// CTRL + SHIFT Shortcuts
				if ((glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_LEFT_SHIFT) || glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_RIGHT_SHIFT))) {// Save Shortcut

					// Save As Shortcuts
					if (glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_S)) {
						savePath = "";
						SaveFile();
					}

					// Node Editor Shortcut
					if (glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_N)) {
						activeWindows.elevationNodeEditorWindow = true;
						noiseBased = false;
					}

					// Noise Layer Shortcut
					if (glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_L)) {
						activeWindows.elevationNodeEditorWindow = false;
						noiseBased = true;
					}

					// Explorer Mode Shortcut
					if (glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_X)) {
						Log("Toggle Explorer Mode");
						isExploreMode = true;
					}

				}
			}

			if (isTextureBake) {
				textureFBO->Begin();
				glViewport(0, 0, 1024, 1024); // This should be 4096 instead of 1024 but my computer is too weak to go for 4096
				GetWindow()->Clear();
				DoTheRederThing(deltatime, false, true);
			}
			else {
				reflectionfbo->Begin();
				glViewport(0, 0, 800, 600);
				GetWindow()->Clear();
				s_Stats.deltaTime = deltatime;
				DoTheRederThing(deltatime);

				glBindFramebuffer(GL_FRAMEBUFFER, GetViewportFramebufferId());
				glViewport(0, 0, 800, 600);
				GetWindow()->Clear();
				DoTheRederThing(deltatime, true);

				if (isPostProcess)
				{
					postProcessFBO->Begin();
					GetWindow()->Clear();
					PostProcess(deltatime);
				}
			}

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			RenderImGui();

			if (autoUpdate)
				RegenerateMesh();

		}
		else {
			static bool expH = false;
			if (!expH) {
				GetWindow()->SetFullScreen(true);
				glfwSetInputMode(GetWindow()->GetNativeWindow(), GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
				ExPSaveCamera(CameraPosition, CameraRotation);
			}


			expH = isExploreMode;



			reflectionfbo->Begin();
			glViewport(0, 0, 800, 600);
			GetWindow()->Clear();
			s_Stats.deltaTime = deltatime;
			DoTheRederThing(deltatime);
			reflectionfbo->End();

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			int w, h;
			glfwGetWindowSize(GetWindow()->GetNativeWindow(), &w, &h);
			glViewport(0, 0, w, h);
			GetWindow()->Clear();
			s_Stats.deltaTime = deltatime;
			UpdateExplorerControls(CameraPosition, CameraRotation, isIExploreMode, &nLOffsetX, &nLOffsetY);
			DoTheRederThing(deltatime, true);

			if (isPostProcess)
			{
				postProcessFBO->Begin();
				PostProcess(deltatime);
			}

			if ((glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_ESCAPE))) {
				isExploreMode = false;
				expH = false;
				glfwSetInputMode(GetWindow()->GetNativeWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				GetWindow()->SetFullScreen(false);
				ExPRestoreCamera(CameraPosition, CameraRotation);
			}


			if (autoUpdate)
				RegenerateMesh();

		}


		if (glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_I))
			noiseGen->offset[0] -= 0.01f;

		if (glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_K))
			noiseGen->offset[0] += 0.01f;

		if (glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_L))
			noiseGen->offset[1] -= 0.01f;

		if (glfwGetKey(GetWindow()->GetNativeWindow(), GLFW_KEY_J))
			noiseGen->offset[1] += 0.01f;


		if (ReqRefresh())
			ResetShader();
	}

	virtual void OnOneSecondTick() override
	{
		if (!isRuinning)
			return;

		secondCounter++;

		if (secondCounter % 5 == 0) {
			if (autoSave) {
				SaveFile(GetExecutableDir() + "\\Data\\cache\\autosave\\autosave.terr3d");

				if (savePath.size() > 3) {
					SaveFile(savePath);
				}
			}
		}

		GetWindow()->SetVSync(vSync);
		s_Stats.frameRate = 1 / s_Stats.deltaTime;
		MeshNodeEditorTick();
		SecondlyShaderEditorUpdate();
		UpdateTextureStore();
		TextureSettingsTick();
	}

	virtual void OnImGuiRender() override
	{
		OnBeforeImGuiRender();
		ShowGeneralControls();
		ShowCameraControls();
		ShowTerrainControls();
		ShowLightingControls();
		ShowNoiseSettings();
		ShowMainScene();
		ShowErrorModal();
		ShowSuccessModal();


		// Optional Windows

		if (activeWindows.statsWindow)
			ShowStats();

		if (activeWindows.texturEditorWindow)
			ShowTextureSettings(&activeWindows.texturEditorWindow);

		if (activeWindows.seaEditor)
			ShowSeaSettings();

		if (activeWindows.modulesManager)
			ShowModuleManager();

		if (activeWindows.styleEditor)
			ShowStyleEditor(&activeWindows.styleEditor);

		if (activeWindows.foliageManager)
			ShowFoliageManager(&activeWindows.foliageManager);

		if (activeWindows.shaderEditorWindow)
			ShowShaderEditor(&activeWindows.shaderEditorWindow);

		if (activeWindows.elevationNodeEditorWindow)
			ShowMeshNodeEditor(&activeWindows.elevationNodeEditorWindow);

		if (activeWindows.textureStore)
			ShowTextureStore(&activeWindows.textureStore);

		if (activeWindows.filtersManager)
			ShowFiltersMamager(&activeWindows.filtersManager);

		if (activeWindows.osLisc)
			ShowOSLiscences(&activeWindows.osLisc);

		if (activeWindows.supportersTribute)
			ShowSupportersTribute(&activeWindows.supportersTribute);

		if (activeWindows.skySettings)
			ShowSkySettings(&activeWindows.skySettings);

		for (UIModule* mod : moduleManager->uiModules)
		{
			if (mod->active)
			{
				ImGui::Begin(mod->windowName.c_str(), &mod->active);
				mod->Render();
				ImGui::End();
			}
		}

		for (NoiseLayerModule* mod : moduleManager->nlModules)
		{
			if (mod->active)
			{
				ImGui::Begin(mod->name.c_str());
				mod->Render();
				ImGui::End();
			}
		}

		OnImGuiRenderEnd();
	}

	virtual void OnStart(std::string loadFile) override
	{
		srand((unsigned int)time(NULL));
		SetUpIcon();
		SetupViewportFrameBuffer();
		SetupShaderManager(GetExecutableDir());
		SetupFoliageManager();
		SetupSupportersTribute();
		SetupExplorerControls();
		SetupTextureStore(GetExecutableDir(), &reqTexRfrsh);
		diffuse = new Texture2D(GetExecutableDir() + "\\Data\\textures\\white.png");
		//gridTex = new Texture2D(GetExecutableDir() + "\\Data\\textures\\grid.png", false, true);
		ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_None;
		LoadDefaultStyle();
		GetWindow()->SetShouldCloseCallback(OnAppClose);
		glfwSetFramebufferSizeCallback(GetWindow()->GetNativeWindow(), [](GLFWwindow* window, int w, int h) {
			glfwSwapBuffers(window);
			});
		glfwSetScrollCallback(GetWindow()->GetNativeWindow(), [](GLFWwindow*, double x, double y) {
			ImGuiIO& io = ImGui::GetIO();
			io.MouseWheel = (float)y;
			});
		GetWindow()->SetClearColor({ 0.1f, 0.1f, 0.1f });
		noiseGen = new LayeredNoiseManager();
		moduleManager = new ModuleManager();
		GenerateMesh();
		SetupMeshNodeEditor(moduleManager);
		glEnable(GL_DEPTH_TEST);
		LightPosition[1] = -0.3f;
		CameraPosition[1] = 0.2f;
		CameraPosition[2] = 3.1f;
		CameraRotation[1] = 2530.0f;
		autoUpdate = false;
		scale = 1;

		if (loadFile.size() > 0) {
			Log("Loading File from " + loadFile);
			OpenSaveFile(loadFile);
		}

		SetProjectId(GenerateId(32));
		SetupFiltersManager(&autoUpdate, &terrain);
		SetupTextureSettings(&reqTexRfrsh, &textureScale, &terrain);
		SetupSky();

		// Load Fonts

		LoadUIFont("Open-Sans-Regular", 18, GetExecutableDir() + "\\Data\\fonts\\OpenSans-Regular.ttf");
		LoadUIFont("OpenSans-Bold", 25, GetExecutableDir() + "\\Data\\fonts\\OpenSans-Bold.ttf");


		waterDudvMap = new Texture2D(GetExecutableDir() + "\\Data\\textures\\water_dudv.png");
		DuDvMapID = "DEFAULT";
		Log("Loaded Water DUDV Map from " + GetExecutableDir() + "\\Data\\textures\\water_dudv.png");
		waterNormal = new Texture2D(GetExecutableDir() + "\\Data\\textures\\water_normal.png");
		waterNormalMap = "DEFAULT";
		Log("Loaded Water DUDV Map from " + GetExecutableDir() + "\\Data\\textures\\water_normal.png");
		reflectionfbo = new FrameBuffer();
		textureFBO = new FrameBuffer(1024, 1024); // This should be 4096 instead of 1024 but my computer is too weak to go for 4096
		LoadTextureThumbs();

		bool tpp = false;

		postProcessFBO = std::make_shared<FrameBuffer>(800, 600);
		postProcessingShader = std::make_shared<Shader>(ReadShaderSourceFile(GetExecutableDir() + "\\Data\\shaders\\post_processing\\vert.glsl", &tpp), ReadShaderSourceFile(GetExecutableDir() + "\\Data\\shaders\\post_processing\\frag.glsl", &tpp));

		// For Debug Only
		autoUpdate = true;

		Log("Started Up App!");
	}

	void OnEnd()
	{
		while (isRemeshing);
		//ShutdownMeshNodeEditor();
		//delete moduleManager;
		delete noiseGen;
		delete foliageShader;
		delete shd;
		delete wireframeShader;
		delete reflectionfbo;
		delete diffuse;
		delete waterDudvMap;
	}
};

Application* CreateApplication()
{
	myApp = new MyApp();
	return myApp;
}