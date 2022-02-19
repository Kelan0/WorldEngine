
#include "core/application/Application.h"
#include "core/application/InputHandler.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Image.h"
#include "core/graphics/Mesh.h"
#include "core/graphics/UniformBuffer.h"
#include "core/graphics/Texture.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/scene/EntityHierarchy.h"
#include "core/engine/scene/Transform.h"
#include "core/engine/scene/Camera.h"
#include "core/engine/scene/event/EventDispacher.h"
#include "core/engine/scene/event/Events.h"
#include "core/engine/renderer/RenderCamera.h"

#include <iostream>

struct UBO0 {
	glm::mat4 modelMatrix = glm::mat4(1.0F);
};
struct UBO1 {
	glm::mat4 viewProjectionMatrix = glm::mat4(1.0F);
};

class App : public Application {

	GraphicsPipelineConfiguration pipelineConfig;
	UniformBuffer* ubo;
	std::shared_ptr<DescriptorPool> descriptorPool;
	Mesh* testMesh = NULL;
	Image2D* testImage = NULL;
	Texture2D* testTexture = NULL;
	RenderCamera camera;
	double cameraPitch = 0.0F;
	double cameraYaw = 0.0F;

	Entity cameraEntity;
	
	void onScreenResize(const ScreenResizeEvent& event) {
		printf("ScreenResizeEvent from [%d x %d] to [%d x %d]\n", event.oldSize.x, event.oldSize.y, event.newSize.x, event.newSize.y);
		//cameraEntity.
	}

	void test(const ScreenResizeEvent& event) {

	}

    void init() override {
		getEventDispacher()->connect<ScreenResizeEvent>(&App::onScreenResize, this);

		cameraEntity = scene()->createEntity("camera");
		cameraEntity.addComponent<Camera>().setPerspective(glm::radians(90.0), graphics()->getAspectRatio(), 0.1, 100.0);
		cameraEntity.addComponent<Transform>();

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
		testTexture = Texture2D::create(testTextureImageViewConfig, testTextureSamplerConfig);

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
		testMesh = Mesh::create(testMeshConfig);


		DescriptorPoolConfiguration descriptorPoolConfig;
		descriptorPoolConfig.device = graphics()->getDevice();
		descriptorPoolConfig.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
		descriptorPool = std::shared_ptr<DescriptorPool>(DescriptorPool::create(descriptorPoolConfig));


		ubo = UniformBuffer::Builder(descriptorPool)
			.addUniformBlock<UBO0>(0, 0, vk::ShaderStageFlagBits::eVertex)
			.addUniformBlock<UBO1>(1, 0, vk::ShaderStageFlagBits::eVertex)
			.addTextureSampler(2, 0, vk::ShaderStageFlagBits::eFragment)
			.build();

		ubo->startBatchWrite(2);
		ubo->writeImage(2, 0, testTexture, vk::ImageLayout::eShaderReadOnlyOptimal);
		ubo->endBatchWrite(2);

		pipelineConfig.vertexShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.vert";
		pipelineConfig.fragmentShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.frag";
		pipelineConfig.vertexInputBindings = Mesh::getVertexBindingDescriptions();
		pipelineConfig.vertexInputAttributes = Mesh::getVertexAttributeDescriptions();
		ubo->initPipelineConfiguration(pipelineConfig);
		graphics()->initializeGraphicsPipeline(pipelineConfig);
		//graphics()->setPreferredPresentMode(vk::PresentModeKHR::eFifo);

		cameraEntity.getComponent<Transform>().setTranslation(0.0F, 2.0F, 2.0F);
    }

	void cleanup() override {
		delete testTexture;
		delete testImage;
		delete testMesh;
		delete ubo;
		descriptorPool.reset();
	}

	void handleUserInput(double dt) {
		if (input()->keyPressed(SDL_SCANCODE_ESCAPE)) {
			input()->toggleMouseGrabbed();
		}

		if (input()->keyPressed(SDL_SCANCODE_F1)) {
			// TODO: not reinitialize the graphics pipeline each time...
			// We should have a dedicated wireframe pipeline and shaders, and just switch to it.
			if (pipelineConfig.polygonMode == vk::PolygonMode::eFill) {
				pipelineConfig.polygonMode = vk::PolygonMode::eLine;
			} else {
				pipelineConfig.polygonMode = vk::PolygonMode::eFill;
			}
			graphics()->initializeGraphicsPipeline(pipelineConfig);
		}

		if (input()->isMouseGrabbed()) {
			Transform& cameraTransform = cameraEntity.getComponent<Transform>();
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

		Camera& projection = cameraEntity.getComponent<Camera>();
		projection.setAspect(graphics()->getAspectRatio());
		projection.setClippingPlanes(0.01F, 500.0F);
		projection.setFov(glm::radians(90.0F));
		camera.setTransform(cameraEntity.getComponent<Transform>());
		camera.setProjection(projection);
		camera.update();

		GraphicsPipeline& pipeline = graphics()->pipeline();
			
		auto& commandBuffer = graphics()->getCurrentCommandBuffer();

		pipeline.bind(commandBuffer);

		glm::mat4 modelMatrix(1.0F);// = glm::translate(glm::mat4(1.0F), glm::vec3(glm::sin(x), 0.0F, 0.0F));

		ubo->update(0, 0, &modelMatrix);
		ubo->update(1, 0, &camera.getViewProjectionMatrix());

		ubo->bind({ 0, 1, 2 }, 0, commandBuffer, pipeline);

		testMesh->draw(commandBuffer);
    }
};

int main(int argc, char* argv[]) {
    return Application::create<App>();
}