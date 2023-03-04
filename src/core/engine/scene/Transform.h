
#ifndef WORLDENGINE_TRANSFORM_H
#define WORLDENGINE_TRANSFORM_H

#include "core/core.h"
#include "core/util/EntityChangeTracker.h"

class Transform {
    friend class SceneRenderer;
public:
    Transform();

    Transform(const glm::mat4& transformationMatrix);

    Transform(const Transform& copy);

    Transform(Transform&& move) noexcept;

    ~Transform();

    const glm::dvec3& getTranslation() const;
    const double& getX() const;
    const double& getY() const;
    const double& getZ() const;

    glm::quat getRotation() const;
    const glm::mat3& getRotationMatrix() const;
    const glm::vec3& getXAxis() const;
    glm::vec3 getLeftAxis() const;
    glm::vec3 getRightAxis() const;
    const glm::vec3& getYAxis() const;
    glm::vec3 getDownAxis() const;
    glm::vec3 getUpAxis() const;
    const glm::vec3& getZAxis() const;
    glm::vec3 getForwardAxis() const;
    glm::vec3 getBackwardAxis() const;
    glm::vec3 getEulerAngles() const;
    float getPitch() const;
    float getYaw() const;
    float getRoll() const;

    const glm::dvec3& getScale() const;
    const double& getScaleX() const;
    const double& getScaleY() const;
    const double& getScaleZ() const;

    Transform& setTranslation(const glm::dvec4& translation);
    Transform& setTranslation(const glm::dvec3& translation);
    Transform& setTranslation(const glm::dvec2& translation);
    Transform& setTranslation(const double& x, const double& y, const double& z);
    Transform& setTranslation(const double& x, const double& y);

    Transform& setRotation(const glm::mat3& rotation);
    Transform& setRotation(const glm::mat4& rotation);
    Transform& setRotation(const glm::vec3& forward, const glm::vec3& up, const bool& normalized = true);
    Transform& setRotation(const glm::quat& rotation, const bool& normalized = true);
    Transform& setRotation(const glm::vec3& eulerAngles);
    Transform& setRotation(const float& pitch, const float& yaw, const float& roll);
    Transform& setRotation(const float& pitch, const float& yaw);

    Transform& setScale(const glm::dvec3& scale);
    Transform& setScale(const double& x, const double& y, const double& z);
    Transform& setScale(const double& scale);

    Transform& translate(const glm::dvec4& translation);
    Transform& translate(const glm::dvec3& translation);
    Transform& translate(const glm::dvec2& translation);
    Transform& translate(const double& x, const double& y, const double& z);
    Transform& translate(const double& x, const double& y);

    Transform& rotate(const glm::quat& rotation, const bool& normalized = true);
    Transform& rotate(const glm::mat3& rotation, const bool& normalized = true);
    Transform& rotate(const glm::mat4& rotation, const bool& normalized = true);
    Transform& rotate(const glm::vec3& axis, const float& angle);
    Transform& rotate(const glm::vec4& axisAngle);
    Transform& rotate(const float& x, const float& y, const float& z, const float& angle);
    Transform& rotate(const glm::vec3& eulerAngles);
    Transform& rotate(const float& pitch, const float& yaw, const float& roll);

    Transform& scale(const glm::dvec3& scale);
    Transform& scale(const double& x, const double& y, const double& z);
    Transform& scale(const double& scale);


    Transform operator*(const glm::dmat4& other) const;
    Transform operator*(const Transform& other) const;

    Transform& operator=(const glm::dmat4& other);
    Transform& operator=(const Transform& other);

    bool equalsTranslation(const Transform& other, const double& epsilon) const;
    bool equalsRotation(const Transform& other, const double& epsilon) const;
    bool equalsScale(const Transform& other, const double& epsilon) const;

    bool equalsTranslation(const Transform& other) const;
    bool equalsRotation(const Transform& other) const;
    bool equalsScale(const Transform& other) const;

    bool operator==(const Transform& other) const;
    bool operator==(const glm::dmat4& other) const;
    bool operator==(const glm::mat4& other) const;

    bool operator!=(const Transform& other) const;
    bool operator!=(const glm::dmat4& other) const;
    bool operator!=(const glm::mat4& other) const;

    glm::dmat4 getMatrix() const;

    static void fillMatrixd(const Transform& transform, glm::dmat4& matrix);

    static void fillMatrixf(const Transform& transform, glm::fmat4& matrix);

    static void fillMatrixf(const Transform& transform1, const Transform& transform2, const double& delta, glm::fmat4& matrix);

    Transform& setMatrix(const glm::dmat4& matrix);

    operator glm::dmat4() const;

private:
//    static void reindex(Transform& transform, const EntityChangeTracker::entity_index& newEntityIndex);

    void change();

private:
    glm::dvec3 m_translation;
    glm::mat3 m_rotation;
    glm::dvec3 m_scale;
    uint64_t m_lastChangedTimestamp;
//    EntityChangeTracker::entity_index m_entityIndex = EntityChangeTracker::INVALID_INDEX;
};


namespace std {
    template<> struct hash<Transform> {
        size_t operator()(const Transform& v) const {
            size_t seed = 0;
            std::hash_combine(seed, v.getTranslation());
            std::hash_combine(seed, v.getEulerAngles());
            std::hash_combine(seed, v.getScale());
            return seed;
        }
    };
}
#endif //WORLDENGINE_TRANSFORM_H
