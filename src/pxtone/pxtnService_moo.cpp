
#include "./pxtn.h"
#include "./pxtnMem.h"
#include "./pxtnService.h"

mooParams::mooParams() {
  b_mute_by_unit = false;
  b_loop = true;

  fade_fade = 0;
  master_vol = 1.0f;
  bt_clock = 0;
  bt_num = 0;

  smp_end = 0;
}

mooState::mooState() {
  group_smps = NULL;
  p_eve = NULL;

  smp_count = 0;
}

void pxtnService::_moo_constructor() {
  _moo_b_init = false;

  _moo_b_valid_data = false;
  _moo_b_end_vomit = true;
  _moo_params = mooParams();
  _moo_state = mooState();
}

bool pxtnService::_moo_release() {
  if (!_moo_b_init) return false;
  _moo_b_init = false;
  _moo_state.release();
  return true;
}
void mooState::release() {
  if (group_smps) free(group_smps);
  group_smps = NULL;
}

void pxtnService::_moo_destructer() { _moo_release(); }

bool pxtnService::_moo_init() {
  bool b_ret = false;

  if (!_moo_state.init(_group_num)) goto term;

  _moo_b_init = true;
  b_ret = true;
term:
  if (!b_ret) _moo_release();

  return b_ret;
}

bool mooState::init(int32_t group_num) {
  return pxtnMem_zero_alloc((void**)&group_smps, sizeof(int32_t) * group_num);
}

////////////////////////////////////////////////
// Units   ////////////////////////////////////
////////////////////////////////////////////////

void mooParams::resetVoiceOn(pxtnUnit* p_u) const {
  p_u->Tone_Reset(bt_tempo, clock_rate);
}

bool pxtnService::_moo_InitUnitTone() {
  if (!_moo_b_init) return false;
  for (int32_t u = 0; u < _unit_num; u++) {
    pxtnUnit* p_u = Unit_Get_variable(u);
    // TODO: Initializing a unit should not take 2 steps over 2 classes.
    if (!p_u->Tone_Init(Woice_Get(EVENTDEFAULT_VOICENO))) return false;
    _moo_params.resetVoiceOn(p_u);
  }
  return true;
}

// u is used to look ahead to cut short notes whose release go into the next.
// This note duration cutting is for the smoothing near the end of a note.
void mooParams::processEvent(pxtnUnit* p_u, int32_t u, const EVERECORD* e,
                             int32_t clock, int32_t dst_ch_num, int32_t dst_sps,
                             const pxtnService* pxtn) const {
  pxtnVOICETONE* p_tone;
  const pxtnWoice* p_wc;
  const pxtnVOICEINSTANCE* p_vi;

  switch (e->kind) {
    case EVENTKIND_ON: {
      int32_t on_count = (int32_t)((e->clock + e->value - clock) * clock_rate);
      if (on_count <= 0) {
        p_u->Tone_ZeroLives();
        break;
      }

      p_u->Tone_KeyOn();

      if (!(p_wc = p_u->get_woice())) break;
      for (int32_t v = 0; v < p_wc->get_voice_num(); v++) {
        p_tone = p_u->get_tone(v);
        p_vi = p_wc->get_instance(v);

        // release..
        if (p_vi->env_release) {
          /* the actual length of the note + release. the (clock - ..->clock)
           * is in case we skip start */
          int32_t max_life_count1 =
              (int32_t)((e->value - (clock - e->clock)) * clock_rate) +
              p_vi->env_release;
          int32_t max_life_count2;
          int32_t c = e->clock + e->value + p_tone->env_release_clock;
          EVERECORD* next = NULL;
          for (EVERECORD* p = e->next; p; p = p->next) {
            if (p->clock > c) break;
            if (p->unit_no == u && p->kind == EVENTKIND_ON) {
              next = p;
              break;
            }
          }
          /* end the note at the end of the song if there's no next note */
          if (!next) max_life_count2 = smp_end - (int32_t)(clock * clock_rate);
          /* end the note at the next note otherwise. */
          else
            max_life_count2 = (int32_t)((next->clock - clock) * clock_rate);
          /* finally, take min of both */
          if (max_life_count1 < max_life_count2)
            p_tone->life_count = max_life_count1;
          else
            p_tone->life_count = max_life_count2;
        }
        // no-release..
        else {
          p_tone->life_count =
              (int32_t)((e->value - (clock - e->clock)) * clock_rate);
        }

        if (p_tone->life_count > 0) {
          p_tone->on_count = on_count;
          p_tone->smp_pos = 0;
          p_tone->env_pos = 0;
          if (p_vi->env_size)
            p_tone->env_volume = p_tone->env_start = 0;  // envelope
          else
            p_tone->env_volume = p_tone->env_start = 128;  // no-envelope
        }
      }
      break;
    }

    case EVENTKIND_KEY:
      p_u->Tone_Key(e->value);
      break;
    case EVENTKIND_PAN_VOLUME:
      p_u->Tone_Pan_Volume(dst_ch_num, e->value);
      break;
    case EVENTKIND_PAN_TIME:
      p_u->Tone_Pan_Time(dst_ch_num, e->value, dst_sps);
      break;
    case EVENTKIND_VELOCITY:
      p_u->Tone_Velocity(e->value);
      break;
    case EVENTKIND_VOLUME:
      p_u->Tone_Volume(e->value);
      break;
    case EVENTKIND_PORTAMENT:
      p_u->Tone_Portament((int32_t)(e->value * clock_rate));
      break;
    case EVENTKIND_BEATCLOCK:
    case EVENTKIND_BEATTEMPO:
    case EVENTKIND_BEATNUM:
    case EVENTKIND_REPEAT:
    case EVENTKIND_LAST:
      break;
    case EVENTKIND_VOICENO: {
      p_u->set_woice(pxtn->Woice_Get(e->value));
      resetVoiceOn(p_u);
    } break;
    case EVENTKIND_GROUPNO:
      p_u->Tone_GroupNo(e->value);
      break;
    case EVENTKIND_TUNING:
      p_u->Tone_Tuning(*((float*)(&e->value)));
      break;
  }
}
// TODO: Could probably put this in _moo_state. Maybe make _moo_params a member
// of it.
bool pxtnService::_moo_PXTONE_SAMPLE(void* p_data) {
  if (!_moo_b_init) return false;

  // envelope..
  for (int32_t u = 0; u < _unit_num; u++) _units[u]->Tone_Envelope();

  int32_t clock = (int32_t)(_moo_state.smp_count / _moo_params.clock_rate);

  /* Adding constant update to moo_smp_end since we might be editing while
   * playing */
  _moo_params.smp_end =
      (int32_t)((double)master->get_play_meas() * _moo_params.bt_num *
                _moo_params.bt_clock * _moo_params.clock_rate);

  /* Notify all the units of events that occurred since the last time increment
     and adjust sampling parameters accordingly */
  // events..
  // TODO: Be able to handle changes while playing.
  // 1. Adding between last and current event for this unit, or deleting the
  // next event, or changing note length.
  // I think _dyn_moo_state.p_eve is incremented past clock at the end of this
  // loop b/c that way, events are only triggered once even if multiple samples
  // have the same clock.
  for (; _moo_state.p_eve && _moo_state.p_eve->clock <= clock;
       _moo_state.p_eve = _moo_state.p_eve->next) {
    int32_t u = _moo_state.p_eve->unit_no;
    _moo_params.processEvent(_units[u], u, _moo_state.p_eve, clock, _dst_ch_num,
                             _dst_sps, this);
  }

  // sampling..
  for (int32_t u = 0; u < _unit_num; u++) {
    _units[u]->Tone_Sample(_moo_params.b_mute_by_unit, _dst_ch_num,
                           _moo_state.time_pan_index, _moo_params.smp_smooth);
  }

  for (int32_t ch = 0; ch < _dst_ch_num; ch++) {
    for (int32_t g = 0; g < _group_num; g++) _moo_state.group_smps[g] = 0;
    /* Sample the units into a group buffer */
    for (int32_t u = 0; u < _unit_num; u++)
      _units[u]->Tone_Supple(_moo_state.group_smps, ch,
                             _moo_state.time_pan_index);
    /* Add overdrive, delay to group buffer */
    for (int32_t o = 0; o < _ovdrv_num; o++)
      _ovdrvs[o]->Tone_Supple(_moo_state.group_smps);
    for (int32_t d = 0; d < _delay_num; d++)
      _delays[d]->Tone_Supple(ch, _moo_state.group_smps);

    /* Add group samples together for final */
    // collect.
    int32_t work = 0;
    for (int32_t g = 0; g < _group_num; g++) work += _moo_state.group_smps[g];

    /* Fading scale probably for rendering at the end */
    // fade..
    if (_moo_params.fade_fade)
      work = work * (_moo_params.fade_count >> 8) / _moo_params.fade_max;

    // master volume
    work = (int32_t)(work * _moo_params.master_vol);

    // to buffer..
    if (work > _moo_params.top) work = _moo_params.top;
    if (work < -_moo_params.top) work = -_moo_params.top;
    *((int16_t*)p_data + ch) = (int16_t)(work);
  }

  // --------------
  // increments..

  _moo_state.smp_count++;
  _moo_state.time_pan_index =
      (_moo_state.time_pan_index + 1) & (pxtnBUFSIZE_TIMEPAN - 1);

  for (int32_t u = 0; u < _unit_num; u++) {
    int32_t key_now = _units[u]->Tone_Increment_Key();
    _units[u]->Tone_Increment_Sample(pxtnPulse_Frequency::Get2(key_now) *
                                     _moo_params.smp_stride);
  }

  // delay
  for (int32_t d = 0; d < _delay_num; d++) _delays[d]->Tone_Increment();

  // fade out
  if (_moo_params.fade_fade < 0) {
    if (_moo_params.fade_count > 0)
      _moo_params.fade_count--;
    else
      return false;
  }
  // fade in
  else if (_moo_params.fade_fade > 0) {
    if (_moo_params.fade_count < (_moo_params.fade_max << 8))
      _moo_params.fade_count++;
    else
      _moo_params.fade_fade = 0;
  }

  if (_moo_state.smp_count >= _moo_params.smp_end) {
    if (!_moo_params.b_loop) return false;
    _moo_state.smp_count = _moo_params.smp_repeat;
    _moo_state.p_eve = evels->get_Records();
    _moo_InitUnitTone();
  }
  return true;
}

///////////////////////
// get / set
///////////////////////

#include <QDebug>
int32_t pxtnService::moo_tone_sample(pxtnUnit* p_u, void* data,
                                     int32_t buf_size,
                                     int32_t time_pan_index) const {
  // TODO: Try to deduplicate this with _moo_PXTONE_SAMPLE
  if (!p_u) return 0;
  if (buf_size < _dst_ch_num) return 0;

  p_u->Tone_Sample(false, _dst_ch_num, time_pan_index, _moo_params.smp_smooth);
  int32_t key_now = p_u->Tone_Increment_Key();
  p_u->Tone_Increment_Sample(pxtnPulse_Frequency::Get2(key_now) *
                             _moo_params.smp_stride);
  for (int ch = 0; ch < _dst_ch_num; ++ch) {
    int32_t work = p_u->Tone_Supple_get(ch, time_pan_index);
    if (work > _moo_params.top) work = _moo_params.top;
    if (work < -_moo_params.top) work = -_moo_params.top;
    *((int16_t*)data + ch) = (int16_t)(work);
  }

  return _dst_ch_num * sizeof(int16_t);
}

bool pxtnService::moo_is_valid_data() const {
  if (!_moo_b_init) return false;
  return _moo_b_valid_data;
}

bool pxtnService::moo_is_end_vomit() const {
  if (!_moo_b_init) return true;
  return _moo_b_end_vomit;
}

/* This place might be a chance to allow variable tempo songs */
int32_t pxtnService::moo_get_now_clock() const {
  if (!_moo_b_init) return 0;
  if (_moo_params.clock_rate)
    return (int32_t)(_moo_state.smp_count / _moo_params.clock_rate);
  return 0;
}

int32_t pxtnService::moo_get_end_clock() const {
  if (!_moo_b_init) return 0;
  if (_moo_params.clock_rate)
    return (int32_t)(_moo_params.smp_end / _moo_params.clock_rate);
  return 0;
}

bool pxtnService::moo_set_mute_by_unit(bool b) {
  if (!_moo_b_init) return false;
  _moo_params.b_mute_by_unit = b;
  return true;
}
bool pxtnService::moo_set_loop(bool b) {
  if (!_moo_b_init) return false;
  _moo_params.b_loop = b;
  return true;
}

bool pxtnService::moo_set_fade(int32_t fade, float sec) {
  if (!_moo_b_init) return false;
  _moo_params.fade_max = (int32_t)((float)_dst_sps * sec) >> 8;
  if (fade < 0) {
    _moo_params.fade_fade = -1;
    _moo_params.fade_count = _moo_params.fade_max << 8;
  }  // out
  else if (fade > 0) {
    _moo_params.fade_fade = 1;
    _moo_params.fade_count = 0;
  }  // in
  else {
    _moo_params.fade_fade = 0;
    _moo_params.fade_count = 0;
  }  // off
  return true;
}

////////////////////////////
// preparation
////////////////////////////

// preparation
bool pxtnService::moo_preparation(const pxtnVOMITPREPARATION* p_prep) {
  if (!_moo_b_init || !_moo_b_valid_data || !_dst_ch_num || !_dst_sps ||
      !_dst_byte_per_smp) {
    _moo_b_end_vomit = true;
    return false;
  }

  bool b_ret = false;
  int32_t start_meas = 0;
  int32_t start_sample = 0;
  float start_float = 0;

  int32_t meas_end = master->get_play_meas();
  int32_t meas_repeat = master->get_repeat_meas();
  float fadein_sec = 0;

  if (p_prep) {
    start_meas = p_prep->start_pos_meas;
    start_sample = p_prep->start_pos_sample;
    start_float = p_prep->start_pos_float;

    if (p_prep->meas_end) meas_end = p_prep->meas_end;
    if (p_prep->meas_repeat) meas_repeat = p_prep->meas_repeat;
    if (p_prep->fadein_sec) fadein_sec = p_prep->fadein_sec;

    if (p_prep->flags & pxtnVOMITPREPFLAG_unit_mute)
      _moo_params.b_mute_by_unit = true;
    else
      _moo_params.b_mute_by_unit = false;
    if (p_prep->flags & pxtnVOMITPREPFLAG_loop)
      _moo_params.b_loop = true;
    else
      _moo_params.b_loop = false;

    _moo_params.master_vol = p_prep->master_volume;
  }

  /* _dst_sps is like samples to seconds. it's set in pxtnService.cpp
     constructor, comes from Main.cpp. 44100 is passed to both this & xaudio. */
  _moo_params.bt_clock = master->get_beat_clock(); /* clock ticks per beat */
  _moo_params.bt_num = master->get_beat_num();
  _moo_params.bt_tempo = master->get_beat_tempo();
  /* samples per clock tick */
  _moo_params.clock_rate =
      (float)(60.0f * (double)_dst_sps /
              ((double)_moo_params.bt_tempo * (double)_moo_params.bt_clock));
  _moo_params.smp_stride = (44100.0f / _dst_sps);
  _moo_params.top = 0x7fff;

  _moo_state.time_pan_index = 0;

  _moo_params.smp_end =
      (int32_t)((double)meas_end * (double)_moo_params.bt_num *
                (double)_moo_params.bt_clock * _moo_params.clock_rate);
  _moo_params.smp_repeat =
      (int32_t)((double)meas_repeat * (double)_moo_params.bt_num *
                (double)_moo_params.bt_clock * _moo_params.clock_rate);

  if (start_float) {
    _moo_params.smp_start =
        (int32_t)((float)moo_get_total_sample() * start_float);
  } else if (start_sample) {
    _moo_params.smp_start = start_sample;
  } else {
    _moo_params.smp_start =
        (int32_t)((double)start_meas * (double)_moo_params.bt_num *
                  (double)_moo_params.bt_clock * _moo_params.clock_rate);
  }

  _moo_state.smp_count = _moo_params.smp_start;
  _moo_params.smp_smooth = _dst_sps / 250;  // (0.004sec) // (0.010sec)

  if (fadein_sec > 0)
    moo_set_fade(1, fadein_sec);
  else
    moo_set_fade(0, 0);

  tones_clear();

  _moo_state.p_eve = evels->get_Records();

  _moo_InitUnitTone();

  b_ret = true;
  _moo_b_end_vomit = false;

  return b_ret;
}

int32_t pxtnService::moo_get_sampling_offset() const {
  if (!_moo_b_init) return 0;
  if (_moo_b_end_vomit) return 0;
  return _moo_state.smp_count;
}

int32_t pxtnService::moo_get_sampling_end() const {
  if (!_moo_b_init) return 0;
  if (_moo_b_end_vomit) return 0;
  return _moo_params.smp_end;
}

int32_t pxtnService::moo_get_total_sample() const {
  if (!_b_init) return 0;
  if (!_moo_b_valid_data) return 0;

  int32_t meas_num;
  int32_t beat_num;
  float beat_tempo;
  master->Get(&beat_num, &beat_tempo, NULL, &meas_num);
  return pxtnService_moo_CalcSampleNum(meas_num, beat_num, _dst_sps,
                                       master->get_beat_tempo());
}

bool pxtnService::moo_set_master_volume(float v) {
  if (!_moo_b_init) return false;
  if (v < 0) v = 0;
  if (v > 1) v = 1;
  _moo_params.master_vol = v;
  return true;
}

////////////////////
// Moo ...
////////////////////

bool pxtnService::Moo(void* p_buf, int32_t size, int32_t* filled_size) {
  if (filled_size) *filled_size = 0;
  if (!_moo_b_init) return false;
  if (!_moo_b_valid_data) return false;
  if (_moo_b_end_vomit) return false;

  bool b_ret = false;

  int32_t smp_w = 0;

  // Testing what happens if mooing takes a long time
  // for (int i = 0, j = 0; i < 20000000; ++i) j += i * i;
  /* No longer failing on remainder - we just return the filled size */
  // if( size % _dst_byte_per_smp ) return false;

  /* Size/smp_num probably is used to sync the playback with the position */
  int32_t smp_num = size / _dst_byte_per_smp;

  {
    /* Buffer is renamed here */
    int16_t* p16 = (int16_t*)p_buf;
    int16_t sample[2]; /* for left and right? */

    /* Iterate thru samples [smp_num] times, fill buffer with this sample */
    for (smp_w = 0; smp_w < smp_num; smp_w++) {
      if (!_moo_PXTONE_SAMPLE(sample)) {
        _moo_b_end_vomit = true;
        break;
      }
      for (int ch = 0; ch < _dst_ch_num; ch++, p16++) *p16 = sample[ch];
    }
    for (; smp_w < smp_num; smp_w++) {
      for (int ch = 0; ch < _dst_ch_num; ch++, p16++) *p16 = 0;
    }

    if (filled_size) *filled_size = smp_num * _dst_byte_per_smp;
  }

  if (_sampled_proc) {
    // int32_t clock = (int32_t)( _dyn_moo_state.smp_count /
    // _moo_state.clock_rate );
    if (!_sampled_proc(_sampled_user, this)) {
      _moo_b_end_vomit = true;
      goto term;
    }
  }

  b_ret = true;
term:
  return b_ret;
}

int32_t pxtnService_moo_CalcSampleNum(int32_t meas_num, int32_t beat_num,
                                      int32_t sps, float beat_tempo) {
  uint32_t total_beat_num;
  uint32_t sample_num;
  if (!beat_tempo) return 0;
  total_beat_num = meas_num * beat_num;
  sample_num = (uint32_t)((double)sps * 60 * (double)total_beat_num /
                          (double)beat_tempo);
  return sample_num;
}
