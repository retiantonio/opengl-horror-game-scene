#include "Camera.hpp"

namespace gps {

    // Camera constructor
    Camera::Camera(glm::vec3 cameraPosition, glm::vec3 cameraTarget, glm::vec3 cameraUp) {
        this->cameraPosition = cameraPosition;
        this->cameraTarget = cameraTarget;
        this->cameraUpDirection = cameraUp;
        this->cameraFrontDirection = glm::normalize(cameraTarget - cameraPosition);
        this->cameraRightDirection = glm::normalize(glm::cross(this->cameraFrontDirection, this->cameraUpDirection));

        // Initialize yaw and pitch based on initial direction
        this->yaw = -90.0f;
        this->pitch = 0.0f;
    }

    // Return the view matrix, using the glm::lookAt() function
    glm::mat4 Camera::getViewMatrix() {
        // We use position, position + front (target), and the up vector
        return glm::lookAt(cameraPosition, cameraPosition + cameraFrontDirection, cameraUpDirection);
    }

    // Update the camera internal parameters following a camera move event
    void Camera::move(MOVE_DIRECTION direction, float speed) {
        switch (direction) {
        case MOVE_FORWARD:
            cameraPosition += cameraFrontDirection * speed;
            break;
        case MOVE_BACKWARD:
            cameraPosition -= cameraFrontDirection * speed;
            break;
        case MOVE_RIGHT:
            cameraPosition += cameraRightDirection * speed;
            break;
        case MOVE_LEFT:
            cameraPosition -= cameraRightDirection * speed;
            break;
        case MOVE_UP:
            cameraPosition += cameraUpDirection * speed;
            break;
        case MOVE_DOWN:
            cameraPosition -= cameraUpDirection * speed;
            break;
        }
        // Target needs to be updated relative to the new position to keep the view consistent
        cameraTarget = cameraPosition + cameraFrontDirection;
    }

    // Update the camera internal parameters following a camera rotate event
    // yaw - camera rotation around the y-axis
    // pitch - camera rotation around the x-axis
    void Camera::rotate(float pitch, float yaw) {
        // Calculate the new Front vector based on Euler angles
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

        cameraFrontDirection = glm::normalize(front);

        // Also re-calculate the Right and Up vector
        // Normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
        cameraRightDirection = glm::normalize(glm::cross(cameraFrontDirection, glm::vec3(0.0f, 1.0f, 0.0f)));
        cameraUpDirection = glm::normalize(glm::cross(cameraRightDirection, cameraFrontDirection));
    }

    glm::vec3 Camera::getCameraPosition() const {
        return cameraPosition;
    }

    glm::vec3 Camera::getCameraFrontDirection() const {
        return cameraFrontDirection;
    }

    glm::vec3 Camera::getCameraRightDirection() const {
        return cameraRightDirection;
    }

    glm::vec3 Camera::getCameraUpDirection() const {
        return cameraUpDirection;
    }

    glm::vec3 Camera::getCameraTarget() const {
        return cameraTarget;
    }

    void Camera::setCameraPosition(glm::vec3 newPosition) {
        cameraPosition = newPosition;
        // Update target to maintain the current looking direction
        cameraTarget = cameraPosition + cameraFrontDirection;
    }

    void Camera::setCameraTarget(glm::vec3 newTarget) {
        cameraTarget = newTarget;
        // Recalculate front direction based on new target
        cameraFrontDirection = glm::normalize(cameraTarget - cameraPosition);
        // Recalculate right and up vectors
        cameraRightDirection = glm::normalize(glm::cross(cameraFrontDirection, glm::vec3(0.0f, 1.0f, 0.0f)));
        cameraUpDirection = glm::normalize(glm::cross(cameraRightDirection, cameraFrontDirection));
    }
}