#include "logging.hpp"
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <SFML/Graphics.hpp>

#if 0
static const float isometric_matrix [16] = {
#if 1
    0.766044, -0.633022, 0.111619, 0.000000,
    0.642788, 0.754407, -0.133022, 0.000000,
    0.000000, 0.173648, 0.984808, 0.000000,
    0.000000, 0.000000, 0.000000, 1.000000,
#else
    0.61237f, -0.50000f, 0.61237f, 0.0f,
    0.35355f,  0.86603f, 0.35355f, 0.0f,
    0.61237f, -0.50000f, 0.61237f, 0.0f,
    0.0f,      0.0f,     0.0f,     1.0f
#endif
};
#endif

static constexpr glm::mat4 transform4_yz {
    1, 0, 0, 0,
    0, 0, 1, 0,
    0, 1, 0, 0,
    0, 0, 0, 1,
};

static constexpr glm::mat4 identity_transform4{1};

static auto make_projection(sf::Vector2f offset, const glm::mat4& transform = identity_transform4)
{
    auto [x, y] = offset;
    auto m = glm::mat4{1};
    m = glm::translate(m, { x, -y, 0 });
    m = glm::scale(m, { 1.f, 0.6f, 1.f });
    m = glm::rotate(m, glm::radians(-45.f), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    m *= transform;
    return m;
}

static const std::string transform_str{"transform"};

int main()
{
    // create the window
    sf::RenderWindow window(sf::VideoMode(800, 800), "My window");
    sf::Texture tex;
    if (!tex.loadFromFile("images/wildtextures_cracked-asphalt-seamless-texture.jpg"))
        return 1;

    sf::Sprite sprite{tex, { { 0, 0 }, { 100, 100 } }};
    sprite.setPosition({0, 0});

    sf::Shader shader;
    if (!shader.loadFromFile("shaders/tile.vert", "shaders/tile.frag"))
        return 1;

    int offx = 0, offy = 0;

    // run the program as long as the window is open
    while (window.isOpen())
    {
        // check all the window's events that were triggered since the last iteration of the loop
        sf::Event event = {};

        while (window.pollEvent(event))
        {
            // "close requested" event: we close the window
            if (event.type == sf::Event::Closed)
                window.close();
            else if (event.type == sf::Event::KeyPressed)
            {
                constexpr int off = 10;
                switch (event.key.code)
                {
                default: break;
                case sf::Keyboard::Escape: window.close(); break;
                case sf::Keyboard::Left:  offx += -off; break;
                case sf::Keyboard::Right: offx += +off; break;
                case sf::Keyboard::Up:    offy += -off; break;
                case sf::Keyboard::Down:  offy += +off; break;
                }
            }
            else if (event.type == sf::Event::Resized)
            {
                auto view = window.getView();
                view.setSize(window.getSize().x, window.getSize().y);
            }
        }

        // clear the window with black color
        window.clear(sf::Color::Black);

        {
            auto [w, h] = window.getView().getSize();
            sf::Vector2f camera_offset { offx / (float)w, offy / (float)h };
            //shader.setUniform(transform_str, make_projection(camera_offset));
            //window.draw(sprite, &shader);
            auto rect = sprite.getLocalBounds();
            sf::Vector2f pos{-(rect.width - rect.left - offx) / w,
                              (rect.height - rect.top + offy)*.5f / h};
            auto mv = make_projection(pos/*, transform4_yz*/);
            shader.setUniform(transform_str, sf::Glsl::Mat4{&mv[0][0]});
            window.draw(sprite, &shader);
        }

        // end the current frame
        window.display();
    }

    return 0;
}
