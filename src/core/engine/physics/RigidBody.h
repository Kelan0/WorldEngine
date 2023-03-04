#ifndef WORLDENGINE_RIGIDBODY_H
#define WORLDENGINE_RIGIDBODY_H

#include "core/engine/scene/Transform.h"

class RigidBody {
    friend class PhysicsSystem;

public:
    enum InterpolationType {
        InterpolationType_None = 0,
        InterpolationType_Interpolate = 1,
        InterpolationType_Extrapolate = 2,
    };
public:
    RigidBody();

    ~RigidBody();

    RigidBody& setTransform(const Transform& transform);

    RigidBody& setInterpolationType(const InterpolationType& interpolationType);

    const Transform& getTransform() const;

    const Transform& getPrevTransform() const;

    const InterpolationType& getInterpolationType() const;

    glm::dvec3 getVelocity() const;

    glm::quat getAngularVelocity() const;

    Transform& transform();

private:
    Transform m_transform;
    Transform m_prevTransform;
    glm::dvec3 m_prevVelocity;
    glm::quat m_prevAngularVelocity;
    InterpolationType m_interpolationType;
};


#endif //WORLDENGINE_RIGIDBODY_H
