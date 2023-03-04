#include "core/engine/physics/RigidBody.h"

RigidBody::RigidBody():
    m_interpolationType(InterpolationType_None) {
}

RigidBody::~RigidBody() {

}

RigidBody& RigidBody::setTransform(const Transform& transform) {
    m_transform = transform;
    return *this;
}

RigidBody& RigidBody::setInterpolationType(const InterpolationType& interpolationType) {
    m_interpolationType = interpolationType;
    return *this;
}

const Transform& RigidBody::getTransform() const {
    return m_transform;
}

const Transform& RigidBody::getPrevTransform() const {
    return m_prevTransform;
}

const RigidBody::InterpolationType& RigidBody::getInterpolationType() const {
    return m_interpolationType;
}

glm::dvec3 RigidBody::getVelocity() const {
    return m_transform.getTranslation() - m_prevTransform.getTranslation();
}

glm::quat RigidBody::getAngularVelocity() const {
    // delta * prev = curr
    // delta = curr * inverse(prev)
    return m_transform.getRotation() * glm::inverse(m_prevTransform.getRotation());
}

Transform& RigidBody::transform() {
    return m_transform;
}