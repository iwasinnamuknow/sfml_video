#include <SFML/Window.hpp>
#include "VideoPlayer.hpp"

int main() {
    sf::RenderWindow window(sf::VideoMode(1280, 720), "Video Example");

    /*
     * If you lower the framerate too much the audio will get choppy.
     * Something to do with buffer size but I didn't investigate too far
     */
    window.setFramerateLimit(120);

    VideoPlayer player("sample.mp4");

    // Example values, similar to sf::Sprite
    player.setPosition(0, 0);
    player.setScale(1, 1);

    while (window.isOpen()) {
        window.clear(sf::Color::Black);

        sf::Event event{};
        while (window.pollEvent(event)) {
            if ((event.type == sf::Event::Closed) || (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Escape)) {
                window.close();
            }
        }

        /*
         * Each time update is called it will fetch one packet from the source
         * and attempt to decode it. The packet may contain more than one frame
         * so typically the reading and decoding will finish long before the playback
         * is finished.
         *
         * This will also cause the next frame to be written to the texture.
         */
        player.update();

        /*
         * Render the builtin sprite
         */
        window.draw(player);

        // Do something when the playback is done
        if (player.isFinished()) {
            window.close();
        }

        window.display();
    }

    return 0;
}