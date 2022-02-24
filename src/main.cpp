
#include "core/application/Application.h"
#include "core/application/InputHandler.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Image.h"
#include "core/graphics/Mesh.h"
#include "core/graphics/ShaderProgram.h"
#include "core/graphics/Texture.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/scene/EntityHierarchy.h"
#include "core/engine/scene/Transform.h"
#include "core/engine/scene/Camera.h"
#include "core/engine/scene/event/EventDispacher.h"
#include "core/engine/scene/event/Events.h"
#include "core/engine/renderer/RenderComponent.h"
#include "core/engine/renderer/RenderCamera.h"

#include <iostream>

struct UniformData {
	glm::mat4 viewProjectionMatrix;
	glm::mat4 modelMatrix;
};

class App : public Application {

	//std::shared_ptr<ShaderProgram> shaderProgram;
	Image2D* testImage = NULL;
	std::shared_ptr<Texture2D> testTexture;
	RenderCamera camera;
	double cameraPitch = 0.0F;
	double cameraYaw = 0.0F;

	std::array<std::shared_ptr<DescriptorSet>, CONCURRENT_FRAMES> globalDescriptorSet;
	std::array<std::shared_ptr<DescriptorSet>, CONCURRENT_FRAMES> objectDescriptorSet;
	std::array<std::shared_ptr<DescriptorSet>, CONCURRENT_FRAMES> materialDescriptorSet;
	std::array<Buffer*, CONCURRENT_FRAMES> cameraInfoBuffer;
	std::array<Buffer*, CONCURRENT_FRAMES> worldTransformBuffer;



	//void onScreenResize(const ScreenResizeEvent& event) {
	//	printf("onScreenResize %f\n", (double)event.newSize.x / (double)event.newSize.y);
	//	cameraEntity.getComponent<Camera>().setAspect((double)event.newSize.x / (double)event.newSize.y);
	//}
	//
	//void onScreenShow(const ScreenShowEvent& event) {
	//	printf("onScreenShow %f\n", (double)event.size.x / (double)event.size.y);
	//	cameraEntity.getComponent<Camera>().setAspect((double)event.size.x / (double)event.size.y);
	//}

    void init() override {
		//eventDispacher()->connect<ScreenResizeEvent>(&App::onScreenResize, this);

		BufferConfiguration uniformDataBufferConfig;
		uniformDataBufferConfig.device = graphics()->getDevice();
		uniformDataBufferConfig.size = sizeof(UniformData);
		uniformDataBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
		uniformDataBufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;

		for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
			cameraInfoBuffer[i] = Buffer::create(uniformDataBufferConfig);
			worldTransformBuffer[i] = Buffer::create(uniformDataBufferConfig);
		}


		DescriptorSetLayoutBuilder builder;

		auto globalDescriptorSetLayout = builder.addUniformBlock(0, vk::ShaderStageFlagBits::eVertex, sizeof(glm::mat4)).build();
		auto objectDescriptorSetLayout = builder.addUniformBlock(0, vk::ShaderStageFlagBits::eVertex, sizeof(glm::mat4)).build();
		auto materialDescriptorSetLayout = builder.addCombinedImageSampler(0, vk::ShaderStageFlagBits::eFragment, 1).build();

		globalDescriptorSetLayout->createDescriptorSets(this->graphics()->descriptorPool(), CONCURRENT_FRAMES, globalDescriptorSet.data());
		objectDescriptorSetLayout->createDescriptorSets(graphics()->descriptorPool(), CONCURRENT_FRAMES, objectDescriptorSet.data());
		materialDescriptorSetLayout->createDescriptorSets(graphics()->descriptorPool(), CONCURRENT_FRAMES, materialDescriptorSet.data());

		//ShaderResources* shaderResources = ShaderResources::Builder()
		//	.addUniformBlock<glm::mat4>(0, 0, vk::ShaderStageFlagBits::eVertex)
		//	.addUniformBlock<glm::mat4>(1, 0, vk::ShaderStageFlagBits::eVertex)
		//	.addTextureSampler(2, 0, vk::ShaderStageFlagBits::eFragment)
		//	.build();

		GraphicsPipelineConfiguration pipelineConfig;
		pipelineConfig.vertexShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.vert";
		pipelineConfig.fragmentShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.frag";
		pipelineConfig.vertexInputBindings = Mesh::getVertexBindingDescriptions();
		pipelineConfig.vertexInputAttributes = Mesh::getVertexAttributeDescriptions();
		pipelineConfig.descriptorSetLayous.emplace_back(globalDescriptorSetLayout->getDescriptorSetLayout());
		pipelineConfig.descriptorSetLayous.emplace_back(objectDescriptorSetLayout->getDescriptorSetLayout());
		pipelineConfig.descriptorSetLayous.emplace_back(materialDescriptorSetLayout->getDescriptorSetLayout());
		//shaderResources->initPipelineConfiguration(pipelineConfig);
		graphics()->configurePipeline(pipelineConfig);

		//shaderProgram = std::shared_ptr<ShaderProgram>(ShaderProgram::create(std::shared_ptr<ShaderResources>(shaderResources), graphics()->pipeline()));



		Image2DConfiguration testTextureImageConfig;
		testTextureImageConfig.device = graphics()->getDevice();
		testTextureImageConfig.filePath = "D:/Code/ActiveProjects/WorldEngine/res/textures/Brick_Wall_017_SD/Brick_Wall_017_basecolor.jpg";
		testTextureImageConfig.usage = vk::ImageUsageFlagBits::eSampled;
		testTextureImageConfig.format = vk::Format::eR8G8B8A8Srgb;
		testImage = Image2D::create(testTextureImageConfig);

		SamplerConfiguration testTextureSamplerConfig;
		testTextureSamplerConfig.device = graphics()->getDevice();
		testTextureSamplerConfig.minFilter = vk::Filter::eLinear;
		testTextureSamplerConfig.magFilter = vk::Filter::eLinear;
		ImageView2DConfiguration testTextureImageViewConfig;
		testTextureImageViewConfig.device = graphics()->getDevice();
		testTextureImageViewConfig.image = testImage->getImage();
		testTextureImageViewConfig.format = testImage->getFormat();
		testTexture = std::shared_ptr<Texture2D>(Texture2D::create(testTextureImageViewConfig, testTextureSamplerConfig));

		MeshData testMeshData;
		//testMeshData.pushTransform();
		//testMeshData.scale(0.5F);
		//for (int i = 0; i < 10; ++i) {
		//	testMeshData.rotateDegrees(36.0F, 0.0F, 0.0F, 1.0F);
		//	testMeshData.pushTransform();
		//	testMeshData.translate(2.0F, 0.0F, 0.0F);
		//	testMeshData.createCuboid(glm::vec3(-0.5F), glm::vec3(0.5F));
		//	testMeshData.popTransform();
		//}
		//testMeshData.popTransform();
		//testMeshData.createUVSphere(glm::vec3(0.0F), 0.5F, 30, 30);

		MeshLoader::loadOBJ("D:/Code/ActiveProjects/WorldEngine/res/meshes/bunny.obj", testMeshData);



		MeshConfiguration testMeshConfig;
		testMeshConfig.device = graphics()->getDevice();
		testMeshConfig.setMeshData(&testMeshData);
		std::shared_ptr<Mesh> testMesh = std::shared_ptr<Mesh>(Mesh::create(testMeshConfig));

		Entity testEntity = EntityHierarchy::create(scene(), "testEntity");
		testEntity.addComponent<Transform>().translate(1.0, 0.0, -2.0);
		testEntity.addComponent<RenderComponent>().setMesh(testMesh);// .setTexture(testTexture);

		for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
			DescriptorSetWriter(globalDescriptorSet[i]).writeBuffer(0, cameraInfoBuffer[i], 0, sizeof(glm::mat4)).write();
			DescriptorSetWriter(objectDescriptorSet[i]).writeBuffer(0, worldTransformBuffer[i], 0, sizeof(glm::mat4)).write();
		}
		//shaderProgram->getShaderResources()->writeImage(2, 0, testTexture.get(), vk::ImageLayout::eShaderReadOnlyOptimal);

		scene()->getMainCamera().getComponent<Transform>().setTranslation(0.0F, 2.0F, 2.0F);
    }

	void cleanup() override {
		for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
			delete cameraInfoBuffer[i];
			delete worldTransformBuffer[i];
			globalDescriptorSet[i].reset();
			objectDescriptorSet[i].reset();
			materialDescriptorSet[i].reset();
		}
		//shaderProgram.reset();

		testTexture.reset();
		delete testImage;
	}

	void handleUserInput(double dt) {
		if (input()->keyPressed(SDL_SCANCODE_ESCAPE)) {
			input()->toggleMouseGrabbed();
		}

		//if (input()->keyPressed(SDL_SCANCODE_F1)) {
		//	// TODO: not reinitialize the graphics pipeline each time...
		//	// We should have a dedicated wireframe pipeline and shaders, and just switch to it.
		//	if (pipelineConfig.polygonMode == vk::PolygonMode::eFill) {
		//		pipelineConfig.polygonMode = vk::PolygonMode::eLine;
		//	} else {
		//		pipelineConfig.polygonMode = vk::PolygonMode::eFill;
		//	}
		//	graphics()->initializeGraphicsPipeline(pipelineConfig);
		//}

		if (input()->isMouseGrabbed()) {
			Transform& cameraTransform = scene()->getMainCamera().getComponent<Transform>();
			glm::ivec2 dMouse = input()->getRelativeMouseState();
			if (isViewportInverted()) 
				dMouse.y *= -1;

			cameraPitch += dMouse.y * 0.001F;
			cameraYaw -= dMouse.x * 0.001F;
			if (cameraYaw > +M_PI) cameraYaw -= M_PI * 2.0;
			if (cameraYaw < -M_PI) cameraYaw += M_PI * 2.0;
			if (cameraPitch > +M_PI * 0.5) cameraPitch = +M_PI * 0.5;
			if (cameraPitch < -M_PI * 0.5) cameraPitch = -M_PI * 0.5;

			cameraTransform.setRotation(cameraPitch, cameraYaw);

			double movementSpeed = 1.0;
			glm::dvec3 movementDir(0.0);

			if (input()->keyDown(SDL_SCANCODE_W)) movementDir.z--;
			if (input()->keyDown(SDL_SCANCODE_S)) movementDir.z++;
			if (input()->keyDown(SDL_SCANCODE_A)) movementDir.x--;
			if (input()->keyDown(SDL_SCANCODE_D)) movementDir.x++;

			if (glm::dot(movementDir, movementDir) > 0.5F) {
				movementDir = cameraTransform.getRotationMatrix() * glm::normalize(movementDir);
				cameraTransform.translate(movementDir * movementSpeed * dt);
			}
		}
	}

    void render(double dt) override {

		handleUserInput(dt);
		
		//const Entity& cameraEntity = scene()->getMainCamera();
		//camera.setTransform(cameraEntity.getComponent<Transform>());
		//camera.setProjection(cameraEntity.getComponent<Camera>());
		//camera.update();
		//cameraInfoBuffer[graphics()->getCurrentFrameIndex()]->upload(0, sizeof(glm::mat4), &camera.getViewProjectionMatrix());
		//
		//auto& commandBuffer = graphics()->getCurrentCommandBuffer();
		//
		//const auto& renderEntities = scene()->registry()->group<RenderComponent>(entt::get<Transform>);
		//
		//
		//for (auto id : renderEntities) {
		//	const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(id);
		//	const Transform& transform = renderEntities.get<Transform>(id);
		//
		//	if (renderComponent.texture != NULL) {
		//		DescriptorSetWriter(materialDescriptorSet[graphics()->getCurrentFrameIndex()])
		//			.writeImage(0, renderComponent.texture.get(), vk::ImageLayout::eShaderReadOnlyOptimal).write();
		//	}
		//
		//	glm::mat4 modelMatrix = transform.getMatrix();
		//	worldTransformBuffer[graphics()->getCurrentFrameIndex()]->upload(0, sizeof(glm::mat4), &modelMatrix);
		//
		//	std::vector<vk::DescriptorSet> descriptorSets = {
		//		globalDescriptorSet[graphics()->getCurrentFrameIndex()]->getDescriptorSet(),
		//		objectDescriptorSet[graphics()->getCurrentFrameIndex()]->getDescriptorSet(),
		//		materialDescriptorSet[graphics()->getCurrentFrameIndex()]->getDescriptorSet(),
		//	};
		//
		//	std::vector<uint32_t> dynamicOffsets = {};
		//
		//	graphics()->pipeline()->bind(commandBuffer);
		//	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics()->pipeline()->getPipelineLayout(), 0, descriptorSets, dynamicOffsets);
		//	renderComponent.mesh->draw(commandBuffer);
		//}
    }
};

int main(int argc, char* argv[]) {
    return Application::create<App>();
}