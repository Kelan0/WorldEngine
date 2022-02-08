#include "Application.h"
#include "graphics/GraphicsManager.h"
#include "graphics/GraphicsPipeline.h"
#include "graphics/Mesh.h"
#include <chrono>

Buffer* ubo = NULL;
std::shared_ptr<DescriptorSet> uboDescriptorSet;
std::shared_ptr<DescriptorPool> descriptorPool;



Application* Application::s_instance = NULL;


Application::Application() {
	m_graphics = new GraphicsManager();
}

Application::~Application() {
	delete m_graphics;

	printf("Destroying window\n");
	SDL_DestroyWindow(m_windowHandle);

	printf("Quitting SDL\n");
	SDL_Quit();

	s_instance = NULL;
	printf("Uninitialized application\n");
}

bool Application::create() {
	printf("Creating application\n");

	assert(s_instance == NULL);
	s_instance = new Application();

	printf("Initializing SDL\n");
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		// Failed initialization
		printf("Failed to initialize SDL: %s\n", SDL_GetError());
		destroy();
		return false;
	}

	printf("Creating window\n");
	uint32_t flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
	s_instance->m_windowHandle = SDL_CreateWindow("Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, flags);
	if (s_instance->m_windowHandle == NULL) {
		// Failed to create window
		printf("Failed to create SDL window: %s\n", SDL_GetError());
		destroy();
		return false;
	}

	if (!s_instance->m_graphics->init(s_instance->m_windowHandle, "WorldEngine")) {
		// Failed to init graphics
		printf("Failed to initialize graphics engine\n");
		destroy();
		return false;
	}

	return true;
}

void Application::destroy() {
	assert(s_instance != NULL);
	delete s_instance;
}

void Application::start() {
	bool running = true;

	uint32_t frameCount = 0;


	BufferConfiguration uboConfiguration;
	uboConfiguration.device = m_graphics->getDevice();
	uboConfiguration.size = sizeof(glm::mat4) * 1;
	uboConfiguration.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
	uboConfiguration.usage = vk::BufferUsageFlagBits::eUniformBuffer;
	ubo = Buffer::create(uboConfiguration);

	DescriptorPoolConfiguration descriptorPoolConfig;
	descriptorPoolConfig.device = m_graphics->getDevice();
	descriptorPoolConfig.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	descriptorPool = DescriptorPool::create(descriptorPoolConfig);

	std::vector<vk::DescriptorSetLayoutBinding> bindings;
	bindings.emplace_back().setBinding(0).setDescriptorType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);

	vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
	layoutCreateInfo.setBindings(bindings);
	uboDescriptorSet = DescriptorSet::get(layoutCreateInfo, descriptorPool);
	DescriptorSetWriter(uboDescriptorSet).writeBuffer(0, ubo).write();

	GraphicsPipelineConfiguration pipelineConfig;
	pipelineConfig.vertexShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.vert";
	pipelineConfig.fragmentShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.frag";
	pipelineConfig.vertexInputBindings = Vertex::getBindingDescriptions();
	pipelineConfig.vertexInputAttributes = Vertex::getAttributeDescriptions();
	pipelineConfig.descriptorSetLayous.push_back(uboDescriptorSet->getLayout()->getDescriptorSetLayout());
	m_graphics->initializeGraphicsPipeline(pipelineConfig);

	auto lastDebug = std::chrono::high_resolution_clock::now();
	//auto lastFrame = std::chrono::high_resolution_clock::now();

	MeshConfiguration testMeshConfig;
	testMeshConfig.device = m_graphics->getDevice();
	testMeshConfig.vertices = {
		{ glm::vec3(-0.5F, -0.5F, 0.0F), glm::vec3(0.0F), glm::vec2(0.0F) },
		{ glm::vec3(+0.5F, -0.5F, 0.0F), glm::vec3(0.0F), glm::vec2(0.0F) },
		{ glm::vec3(+0.5F, +0.5F, 0.0F), glm::vec3(0.0F), glm::vec2(0.0F) },
		{ glm::vec3(-0.5F, +0.5F, 0.0F), glm::vec3(0.0F), glm::vec2(0.0F) }
	};

	testMeshConfig.indices = {
		0, 1, 2, 0, 2, 3
	};

	Mesh* testMesh = Mesh::create(testMeshConfig);

	float x = 0.0F;

	while (running) {
		auto frameStart = std::chrono::high_resolution_clock::now();

		SDL_Event event;

		while (running && SDL_PollEvent(&event)) {

			if (event.type == SDL_QUIT) {
				running = false;
			}
		
		}

		vk::CommandBuffer commandBuffer;
		vk::Framebuffer framebuffer;
		if (m_graphics->beginFrame(commandBuffer, framebuffer)) {

			GraphicsPipeline& pipeline = m_graphics->pipeline();

			vk::ClearValue clearValue;
			clearValue.color.setFloat32({ 0.0F, 0.0F, 0.0F, 1.0F });

			vk::RenderPassBeginInfo renderPassBeginInfo;
			renderPassBeginInfo.setRenderPass(pipeline.getRenderPass());
			renderPassBeginInfo.setFramebuffer(framebuffer);
			renderPassBeginInfo.renderArea.setOffset({ 0, 0 });
			renderPassBeginInfo.renderArea.setExtent(m_graphics->getImageExtent());
			renderPassBeginInfo.setClearValueCount(1);
			renderPassBeginInfo.setPClearValues(&clearValue);

			commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
			pipeline.bind(commandBuffer);
			//commandBuffer.draw(3, 1, 0, 0);

			glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0F), glm::vec3(glm::sin(x), 0.0F, 0.0F));
			x += 1.0F / 8000.0F;
			ubo->upload(0, 1, &modelMatrix);

			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,pipeline.getPipelineLayout(), 0, 1, &uboDescriptorSet->getDescriptorSet(), 0, NULL);

			testMesh->draw(commandBuffer);

			commandBuffer.endRenderPass();

			m_graphics->endFrame();
		}

		++frameCount;


		uint64_t debugDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(frameStart - lastDebug).count();

		if (debugDuration >= 1000000000u) {
			double secondsElapsed = debugDuration / 1000000000.0;
			printf("%.2f FPS\n", frameCount / secondsElapsed);
			frameCount = 0;
			lastDebug = frameStart;
		}
	}

	m_graphics->getDevice()->waitIdle();

	delete testMesh;
	delete ubo;
	uboDescriptorSet.reset();
	descriptorPool.reset();
}

Application* Application::instance() {
	return s_instance;
}

GraphicsManager* Application::graphics() {
	return m_graphics;
}

glm::ivec2 Application::getWindowSize() const {
	glm::ivec2 size;
	SDL_GetWindowSize(m_windowHandle, &size.x, &size.y);
	return size;
}
