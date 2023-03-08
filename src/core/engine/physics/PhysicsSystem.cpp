#include "core/engine/physics/PhysicsSystem.h"
#include "core/engine/physics/RigidBody.h"
#include "core/engine/scene/Scene.h"
#include "core/application/Engine.h"

PhysicsSystem::PhysicsSystem():
    m_scene(nullptr) {
}

PhysicsSystem::~PhysicsSystem() {

}

bool PhysicsSystem::init() {
    m_scene->enableEvents<RigidBody>();
    m_scene->getEventDispatcher()->connect<ComponentAddedEvent<RigidBody>>(&PhysicsSystem::onRigidBodyAdded, this);
    m_scene->getEventDispatcher()->connect<ComponentRemovedEvent<RigidBody>>(&PhysicsSystem::onRigidBodyRemoved, this);
    return true;
}

void PhysicsSystem::preTick(double dt) {
    PROFILE_SCOPE("PhysicsSystem::preTick");
    const auto& physicsEntities = m_scene->registry()->view<RigidBody>();

    for (auto it = physicsEntities.begin(); it != physicsEntities.end(); ++it) {
        RigidBody& rigidBody = physicsEntities.get<RigidBody>(*it);

        rigidBody.m_prevVelocity = rigidBody.getVelocity();
        rigidBody.m_prevAngularVelocity = rigidBody.getAngularVelocity();
        rigidBody.m_prevTransform = rigidBody.m_transform;
    }
}

void PhysicsSystem::tick(double dt) {
    PROFILE_SCOPE("PhysicsSystem::tick");
}

void PhysicsSystem::preRender(double dt) {
    PROFILE_SCOPE("PhysicsSystem::preRender");
    const auto& physicsEntities = m_scene->registry()->view<RigidBody, Transform>();

    double partialTicks = Engine::instance()->getPartialTicks();
    float fpartialTicks = (float)partialTicks;

    for (auto it = physicsEntities.begin(); it != physicsEntities.end(); ++it) {
        RigidBody& rigidBody = physicsEntities.get<RigidBody>(*it);
        Transform& transform = physicsEntities.get<Transform>(*it);

        switch (rigidBody.getInterpolationType()) {
            case RigidBody::InterpolationType_None:
                transform = rigidBody.getTransform();
                break;

            case RigidBody::InterpolationType_Interpolate:
                transform.setTranslation(glm::lerp(rigidBody.getPrevTransform().getTranslation(), rigidBody.getTransform().getTranslation(), partialTicks));
                transform.setRotation(glm::slerp(rigidBody.getPrevTransform().getRotation(), rigidBody.getTransform().getRotation(), fpartialTicks));
                transform.setScale(glm::lerp(rigidBody.getPrevTransform().getScale(), rigidBody.getTransform().getScale(), partialTicks));
                break;

            case RigidBody::InterpolationType_Extrapolate:
                transform.setTranslation(rigidBody.getTransform().getTranslation() + rigidBody.getVelocity() * partialTicks);

                glm::quat nextRotation = rigidBody.getAngularVelocity() * rigidBody.getTransform().getRotation();
                transform.setRotation(glm::slerp(rigidBody.getTransform().getRotation(), nextRotation, fpartialTicks));

                // Scale is not extrapolated, only interpolated
                transform.setScale(glm::lerp(rigidBody.getPrevTransform().getScale(), rigidBody.getTransform().getScale(), partialTicks));
                break;

        }
    }
}

void PhysicsSystem::setScene(Scene* scene) {
    m_scene = scene;
}

Scene* PhysicsSystem::getScene() const {
    return m_scene;
}

void PhysicsSystem::onRigidBodyAdded(ComponentAddedEvent<RigidBody>* event) {
    if (!event->entity.hasComponent<Transform>()) {
        Transform& transform = event->entity.addComponent<Transform>();
//        transform = event->component->getTransform();
    }
}

void PhysicsSystem::onRigidBodyRemoved(ComponentRemovedEvent<RigidBody>* event) {

}