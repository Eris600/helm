/* Copyright 2013-2017 Matt Tytel
 *
 * helm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * helm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with helm.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "open_gl_wave_viewer.h"

#include "colors.h"
#include "synth_gui_interface.h"

#define GRID_CELL_WIDTH 8
#define FRAMES_PER_SECOND 30
#define PADDING 5.0f
#define MARKER_WIDTH 12.0f
#define NOISE_RESOLUTION 6
#define IMAGE_WIDTH 256
#define IMAGE_HEIGHT 128

namespace {
  static const float random_values[NOISE_RESOLUTION] = {0.3, 0.9, -0.9, -0.2, -0.5, 0.7 };
} // namespace

OpenGlWaveViewer::OpenGlWaveViewer(int resolution) {
  wave_slider_ = nullptr;
  amplitude_slider_ = nullptr;
  resolution_ = resolution;
  wave_phase_ = nullptr;
  wave_amp_ = nullptr;
}

OpenGlWaveViewer::~OpenGlWaveViewer() {
  openGLContext.detach();
}

void OpenGlWaveViewer::paintBackground() {
  if (getWidth() <= 0 || getHeight() <= 0)
    return;

  const Desktop::Displays::Display& display = Desktop::getInstance().getDisplays().getMainDisplay();
  float scale = display.scale;

  float image_width = scale * IMAGE_WIDTH;
  float image_height = scale * IMAGE_HEIGHT;
  Image background = Image(Image::ARGB, image_width, image_height, true);
  Graphics g(background);
  g.addTransform(AffineTransform::scale(image_width / getWidth(), image_height / getHeight()));

  static const DropShadow shadow(Colour(0xbb000000), 5, Point<int>(0, 0));

  g.fillAll(Colour(0xff424242));

  g.setColour(Colour(0xff4a4a4a));
  for (int x = 0; x < getWidth(); x += GRID_CELL_WIDTH)
    g.drawLine(x, 0, x, getHeight());
  for (int y = 0; y < getHeight(); y += GRID_CELL_WIDTH)
    g.drawLine(0, y, getWidth(), y);

  shadow.drawForPath(g, wave_path_);

  g.setColour(Colors::graphFill);
  g.fillPath(wave_path_);

  g.setColour(Colors::modulation);
  g.strokePath(wave_path_, PathStrokeType(1.5f, PathStrokeType::beveled, PathStrokeType::rounded));

  updateBackgroundImage(background);
}

void OpenGlWaveViewer::paintPositionImage() {
  int min_image_width = roundToInt(2 * MARKER_WIDTH);
  int image_width = roundToInt(powf(2.0f, (int)(log2f(min_image_width))));
  int marker_width = MARKER_WIDTH;
  int image_height = roundToInt(2 * IMAGE_HEIGHT);
  position_image_ = Image(Image::ARGB, image_width, image_height, true);
  Graphics g(position_image_);

  g.setColour(Colour(0x77ffffff));
  g.fillRect(image_width / 2.0f - 0.5f, 0.0f, 1.0f, 1.0f * image_height);

  g.setColour(Colors::modulation);
  g.fillEllipse(image_width / 2 - marker_width / 2, image_height / 2 - marker_width / 2,
                marker_width, marker_width);
  g.setColour(Colour(0xff000000));
  g.fillEllipse(image_width / 2 - marker_width / 4, image_height / 2 - marker_width / 4,
                marker_width / 2, marker_width / 2);
}

void OpenGlWaveViewer::resized() {
  resetWavePath();
}

void OpenGlWaveViewer::mouseDown(const MouseEvent& e) {
  if (wave_slider_) {
    int current_value = wave_slider_->getValue();
    if (e.mods.isRightButtonDown())
      current_value = current_value + wave_slider_->getMaximum();
    else
      current_value = current_value + 1;
    wave_slider_->setValue(current_value % static_cast<int>(wave_slider_->getMaximum() + 1));

    resetWavePath();
  }
}

void OpenGlWaveViewer::setWaveSlider(Slider* slider) {
  if (wave_slider_)
    wave_slider_->removeListener(this);
  wave_slider_ = slider;
  wave_slider_->addListener(this);
  resetWavePath();
}

void OpenGlWaveViewer::setAmplitudeSlider(Slider* slider) {
  if (amplitude_slider_)
    amplitude_slider_->removeListener(this);
  amplitude_slider_ = slider;
  amplitude_slider_->addListener(this);
  resetWavePath();
}

void OpenGlWaveViewer::drawRandom() {
  float amplitude = amplitude_slider_ ? amplitude_slider_->getValue() : 1.0f;
  float draw_width = getWidth();
  float draw_height = getHeight() - 2.0f * PADDING;

  wave_path_.startNewSubPath(0, getHeight() / 2.0f);
  for (int i = 0; i < NOISE_RESOLUTION; ++i) {
    float t1 = (1.0f * i) / NOISE_RESOLUTION;
    float t2 = (1.0f + i) / NOISE_RESOLUTION;
    float val = amplitude * random_values[i];
    wave_path_.lineTo(t1 * draw_width, PADDING + draw_height * ((1.0f - val) / 2.0f));
    wave_path_.lineTo(t2 * draw_width, PADDING + draw_height * ((1.0f - val) / 2.0f));
  }

  wave_path_.lineTo(getWidth(), getHeight() / 2.0f);
}

void OpenGlWaveViewer::drawSmoothRandom() {
  float amplitude = amplitude_slider_ ? amplitude_slider_->getValue() : 1.0f;
  float draw_width = getWidth();
  float draw_height = getHeight() - 2.0f * PADDING;

  float start_val = amplitude * random_values[0];
  wave_path_.startNewSubPath(-50, getHeight() / 2.0f);
  wave_path_.lineTo(0, PADDING + draw_height * ((1.0f - start_val) / 2.0f));
  for (int i = 1; i < resolution_ - 1; ++i) {
    float t = (1.0f * i) / resolution_;
    float phase = t * (NOISE_RESOLUTION - 1);
    int index = (int)phase;
    phase = mopo::PI * (phase - index);
    float val = amplitude * INTERPOLATE(random_values[index],
                                        random_values[index + 1],
                                        0.5 - cos(phase) / 2.0);
    wave_path_.lineTo(t * draw_width, PADDING + draw_height * ((1.0f - val) / 2.0f));
  }

  float end_val = amplitude * random_values[NOISE_RESOLUTION - 1];
  wave_path_.lineTo(getWidth(), PADDING + draw_height * ((1.0f - end_val) / 2.0f));
  wave_path_.lineTo(getWidth() + 50, getHeight() / 2.0f);

}

void OpenGlWaveViewer::resetWavePath() {
  wave_path_.clear();

  if (wave_slider_ == nullptr)
    return;

  float amplitude = amplitude_slider_ ? amplitude_slider_->getValue() : 1.0f;
  float draw_width = getWidth();
  float draw_height = getHeight() - 2.0f * PADDING;

  mopo::Wave::Type type = static_cast<mopo::Wave::Type>(static_cast<int>(wave_slider_->getValue()));

  if (type < mopo::Wave::kWhiteNoise) {
    wave_path_.startNewSubPath(0, getHeight() / 2.0f);
    for (int i = 1; i < resolution_ - 1; ++i) {
      float t = (1.0f * i) / resolution_;
      float val = amplitude * mopo::Wave::wave(type, t);
      wave_path_.lineTo(t * draw_width, PADDING + draw_height * ((1.0f - val) / 2.0f));
    }

    wave_path_.lineTo(getWidth(), getHeight() / 2.0f);
  }
  else if (type == mopo::Wave::kWhiteNoise)
    drawRandom();
  else
    drawSmoothRandom();
  
  paintBackground();
}

void OpenGlWaveViewer::sliderValueChanged(Slider* sliderThatWasMoved) {
  resetWavePath();
}

void OpenGlWaveViewer::showRealtimeFeedback(bool show_feedback) {
  OpenGlComponent::showRealtimeFeedback(show_feedback);

  if (show_feedback) {
    SynthGuiInterface* parent = findParentComponentOfClass<SynthGuiInterface>();
    if (parent) {
      wave_amp_ = parent->getSynth()->getModSource(getName().toStdString());
      wave_phase_ = parent->getSynth()->getModSource(getName().toStdString() + "_phase");
    }
  }
  else
    wave_phase_ = nullptr;
}

float OpenGlWaveViewer::phaseToX(float phase) {
  return phase * getWidth();
}

void OpenGlWaveViewer::init() {
  paintPositionImage();

  position_vertices_ = new float[16] {
    0.0f, 1.0f, 0.0f, 1.0f,
    0.0f, -1.0f, 0.0f, 0.0f,
    0.1f, -1.0f, 1.0f, 0.0f,
    0.1f, 1.0f, 1.0f, 1.0f
  };

  position_triangles_ = new int[6] {
    0, 1, 2,
    2, 3, 0
  };

  openGLContext.extensions.glGenBuffers(1, &vertex_buffer_);
  openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  GLsizeiptr vert_size = static_cast<GLsizeiptr>(static_cast<size_t>(16 * sizeof(float)));
  openGLContext.extensions.glBufferData(GL_ARRAY_BUFFER, vert_size,
                                        position_vertices_, GL_STATIC_DRAW);

  openGLContext.extensions.glGenBuffers(1, &triangle_buffer_);
  openGLContext.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle_buffer_);

  GLsizeiptr tri_size = static_cast<GLsizeiptr>(static_cast<size_t>(6 * sizeof(float)));
  openGLContext.extensions.glBufferData(GL_ELEMENT_ARRAY_BUFFER, tri_size,
                                        position_triangles_, GL_STATIC_DRAW);

}

void OpenGlWaveViewer::drawPosition() {
  float x = 2.0f * wave_phase_->buffer[0] - 1.0f;
  float y = (getHeight() - 2 * PADDING) * wave_amp_->buffer[0] / getHeight();

  float desktop_scale = openGLContext.getRenderingScale();

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  int draw_width = roundToInt(desktop_scale * getWidth());
  int draw_height = roundToInt(desktop_scale * getHeight());

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  float position_height = (1.0f * position_texture_.getHeight()) / draw_height;
  float position_width = (1.0f * position_texture_.getWidth()) / draw_width;
  position_vertices_[0] = x - position_width;
  position_vertices_[1] = y + position_height;
  position_vertices_[4] = x - position_width;
  position_vertices_[5] = y - position_height;
  position_vertices_[8] = x + position_width;
  position_vertices_[9] = y - position_height;
  position_vertices_[12] = x + position_width;
  position_vertices_[13] = y + position_height;

  openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  GLsizeiptr vert_size = static_cast<GLsizeiptr>(static_cast<size_t>(16 * sizeof(float)));
  openGLContext.extensions.glBufferData(GL_ARRAY_BUFFER, vert_size,
                                        position_vertices_, GL_STATIC_DRAW);

  openGLContext.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle_buffer_);
  position_texture_.bind();

  openGLContext.extensions.glActiveTexture(GL_TEXTURE0);
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH);

  if (texture_uniform_ != nullptr)
    texture_uniform_->set(0);

  image_shader_->use();

  enableAttributes();
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  disableAttributes();

  position_texture_.unbind();

  glDisable(GL_TEXTURE_2D);

  openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, 0);
  openGLContext.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void OpenGlWaveViewer::render() {
  if (position_texture_.getWidth() != position_image_.getWidth())
    position_texture_.loadImage(position_image_);

  drawBackground();
  if (animating_)
    drawPosition();
}

void OpenGlWaveViewer::destroy() {
  position_texture_.release();

  texture_ = nullptr;
  openGLContext.extensions.glDeleteBuffers(1, &vertex_buffer_);
  openGLContext.extensions.glDeleteBuffers(1, &triangle_buffer_);
}