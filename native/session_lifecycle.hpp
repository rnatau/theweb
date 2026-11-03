#pragma once
#include "game_session.hpp"
#include <memory>
namespace web {
// Both desktop hosts replace a drained game through the same ordered handoff.
// Devices and the renderer remain host-owned; the fresh session receives neutral
// input through its ordinary begin-ball/serve frames before input is re-armed.
template <class Factory> Scores restart_session(std::unique_ptr<GameSession>& game, Factory create, SoundSink* audio, unsigned music_policy, MusicContinuation& music) {
    const auto records = game->save_scores();
    const auto playing = music.playback(game->music_playback());
    game->sound_sink(nullptr);
    if (audio)
        audio->stop();
    game = create();
    game->load_scores(records);
    game->music_restart_policy(music_policy);
    game->sound_sink(audio);
    game->start();
    game->step(0);
    game->step(0);
    music.begin(playing, game->music_playback());
    return records;
}
template <class Output> void update_session_music(GameSession& game, Output& output, MusicContinuation& music, unsigned volume, bool suspended) {
    if (const auto completed = output.completed())
        game.music_completed(music.completed(*completed, game.music_playback()));
    output.update(music.playback(game.music_playback()), volume);
    output.pause(suspended);
}
} // namespace web
