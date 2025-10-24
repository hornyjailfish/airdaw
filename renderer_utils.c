#include "raylib.h"
#include "raymath.h"
#include "vendor/clay/clay.h"
#include <math.h>

#define CLAY_RECTANGLE_TO_RAYLIB_RECTANGLE(rectangle)                          \
  (Rectangle) {                                                                \
    .x = rectangle.x, .y = rectangle.y, .width = rectangle.width,              \
    .height = rectangle.height                                                 \
  }
#define CLAY_COLOR_TO_RAYLIB_COLOR(color)                                      \
  (Color) {                                                                    \
    .r = (unsigned char)roundf(color.r), .g = (unsigned char)roundf(color.g),  \
    .b = (unsigned char)roundf(color.b), .a = (unsigned char)roundf(color.a)   \
  }

void HandleClayErrors(Clay_ErrorData errorData) {
  // See the Clay_ErrorData struct for more information
  switch (errorData.errorType) {
  case CLAY_ERROR_TYPE_TEXT_MEASUREMENT_FUNCTION_NOT_PROVIDED:
  case CLAY_ERROR_TYPE_ARENA_CAPACITY_EXCEEDED:
  case CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED:
  case CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED:
  case CLAY_ERROR_TYPE_DUPLICATE_ID:
  case CLAY_ERROR_TYPE_FLOATING_CONTAINER_PARENT_NOT_FOUND:
  case CLAY_ERROR_TYPE_PERCENTAGE_OVER_1:
  case CLAY_ERROR_TYPE_INTERNAL_ERROR:
    break;
  default:
    TraceLog(LOG_ERROR, "[clay] %s: %s", errorData.errorType,
             errorData.errorText.chars);
    break;
  }
}

// Get a ray trace from the screen position (i.e mouse) within a specific
// section of the screen
Ray GetScreenToWorldPointWithZDistance(Vector2 position, Camera camera,
                                       int screenWidth, int screenHeight,
                                       float zDistance) {
  Ray ray = {0};

  // Calculate normalized device coordinates
  // NOTE: y value is negative
  float x = ((2.0F * position.x) / (float)screenWidth) - 1.0F;
  float y = 1.0F - ((2.0F * position.y) / (float)screenHeight);
  float z = 1.0F;

  // Store values in a vector
  Vector3 deviceCoords = {x, y, z};

  // Calculate view matrix from camera look at
  Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);

  Matrix matProj = MatrixIdentity();

  if (camera.projection == CAMERA_PERSPECTIVE) {
    // Calculate projection matrix from perspective
    matProj = MatrixPerspective(camera.fovy * DEG2RAD,
                                ((double)screenWidth / (double)screenHeight),
                                0.01F, zDistance);
  } else if (camera.projection == CAMERA_ORTHOGRAPHIC) {
    double aspect = (double)screenWidth / (double)screenHeight;
    double top = camera.fovy / 2.0F;
    double right = top * aspect;

    // Calculate projection matrix from orthographic
    matProj = MatrixOrtho(-right, right, -top, top, 0.01, 1000.0);
  }

  // Unproject far/near points
  Vector3 nearPoint = Vector3Unproject(
      (Vector3){deviceCoords.x, deviceCoords.y, 0.0F}, matProj, matView);
  Vector3 farPoint = Vector3Unproject(
      (Vector3){deviceCoords.x, deviceCoords.y, 1.0F}, matProj, matView);

  // Calculate normalized direction vector
  Vector3 direction = Vector3Normalize(Vector3Subtract(farPoint, nearPoint));

  ray.position = farPoint;

  // Apply calculated vectors to ray
  ray.direction = direction;

  return ray;
}
