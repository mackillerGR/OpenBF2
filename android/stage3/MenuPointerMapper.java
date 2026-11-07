package com.mackillergr.bf2;

/**
 * Pure geometry for Android overlay->SDL pointer conversion.
 *
 * View pixel coordinates are transformed through the letterbox/fitted game
 * content rectangle to game coordinates, and then back into SDL's window
 * coordinates. Win32OnSdl.cpp maps SDL mouse positions to BF2 engine sizes.
 * With Stage3.7's native 2340x1080 launch resolution the transform is 1:1.
 */
public final class MenuPointerMapper {
    private MenuPointerMapper() { }

    public static float[] map(float x, float y,
                              int viewWidth, int viewHeight,
                              int gameWidth, int gameHeight) {
        if (viewWidth <= 0 || viewHeight <= 0 || gameWidth <= 0 || gameHeight <= 0)
            return new float[] {x, y};
        float scale = Math.min(viewWidth / (float) gameWidth, viewHeight / (float) gameHeight);
        if (!(scale > 0)) return new float[] {x, y};
        float contentWidth = gameWidth * scale;
        float contentHeight = gameHeight * scale;
        float offsetX = (viewWidth - contentWidth) / 2f;
        float offsetY = (viewHeight - contentHeight) / 2f;
        float gameX = (x - offsetX) / scale;
        float gameY = (y - offsetY) / scale;
        gameX = Math.max(0f, Math.min(gameWidth - 1f, gameX));
        gameY = Math.max(0f, Math.min(gameHeight - 1f, gameY));
        // SDL's absolute mouse event takes window coordinates, not game pixels.
        return new float[] {
            gameX * viewWidth / gameWidth,
            gameY * viewHeight / gameHeight
        };
    }
}
