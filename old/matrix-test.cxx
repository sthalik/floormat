#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale
#include <cstdio>
int main(void)
{
    glm::mat4 m(1);
    m = glm::rotate(m, glm::radians(-45.f), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::scale(m, { 0.68, 1, 1 });
    for (int j = 0; j < 4; j++)
        printf("%f, %f, %f, %f,\n", m[j][0], m[j][1], m[j][2], m[j][3]);
    return 0;
}
