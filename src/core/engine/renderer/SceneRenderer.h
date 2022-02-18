#pragma once

#include "../../core.h"

class Scene;
class RenderCamera;

class SceneRenderer {
public:
	SceneRenderer();

	~SceneRenderer();

	void render(double dt);

	void setScene(Scene* scene);

	Scene* getScene() const;

private:
	Scene* m_scene;
};

