/*
 * Copyright (C) 2000-2004 James Courtier-Dutton
 * Copyright (C) 2005 Nathan Hurst
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <ctype.h>
#include <stdbool.h>
#include <pthread.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <math.h>

#include "main.h"
#include "errors.h"

static void generate_sine(uint8_t *frames, int count, double *_phase, double _freq, unsigned int _rate, unsigned int _channels, bool _enable) {
  double phase = *_phase;
  double max_phase = 1.0 / _freq;
  double step = 1.0 / (double)_rate;
  double res = 0.0;
  unsigned int    chn;
  int32_t  ires;
  int16_t *samp16 = (int16_t*) frames;

  while (count-- > 0) {
    for(chn = 0; chn < _channels; chn++) {
      // SND_PCM_FORMAT_S16_LE:
      if(_enable)
      {
        res = (sin((phase * 2 * M_PI) / max_phase - M_PI)) * 0x03fffffff; /* Don't use MAX volume */
      }
      ires = res;
      *samp16++ = ires >> 16;
    }

    phase += step;
    if (phase >= max_phase)
      phase -= max_phase;
  }

  *_phase = phase;
}

static int set_hwparams(
    snd_pcm_t *handle,
    snd_pcm_hw_params_t *params,
    snd_pcm_access_t access,
    snd_pcm_format_t format,
    unsigned int rate,
    unsigned int channels,
    snd_pcm_uframes_t *buffer_size,
    snd_pcm_uframes_t *period_size) {
  unsigned int rrate;
  int          err;
  snd_pcm_uframes_t     period_size_min;
  snd_pcm_uframes_t     period_size_max;
  snd_pcm_uframes_t     buffer_size_min;
  snd_pcm_uframes_t     buffer_size_max;
  unsigned int       period_time = 0;              /* period time in us */
  unsigned int       buffer_time = 0;              /* ring buffer length in us */
  unsigned int       nperiods    = 4;                  /* number of periods */

  /* choose all parameters */
  err = snd_pcm_hw_params_any(handle, params);
  if (err < 0) {
    fprintf(stderr, "Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
    return err;
  }

  /* set the interleaved read/write format */
  err = snd_pcm_hw_params_set_access(handle, params, access);
  if (err < 0) {
    fprintf(stderr, "Access type not available for playback: %s\n", snd_strerror(err));
    return err;
  }

  /* set the sample format */
  err = snd_pcm_hw_params_set_format(handle, params, format);
  if (err < 0) {
    fprintf(stderr, "Sample format not available for playback: %s\n", snd_strerror(err));
    return err;
  }

  /* set the count of channels */
  err = snd_pcm_hw_params_set_channels(handle, params, channels);
  if (err < 0) {
    fprintf(stderr, "Channels count (%i) not available for playbacks: %s\n", channels, snd_strerror(err));
    return err;
  }

  /* set the stream rate */
  rrate = rate;
  err = snd_pcm_hw_params_set_rate(handle, params, rate, 0);
  if (err < 0) {
    fprintf(stderr, "Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
    return err;
  }

  if (rrate != rate) {
    fprintf(stderr, "Rate doesn't match (requested %iHz, get %iHz, err %d)\n", rate, rrate, err);
    return -EINVAL;
  }

  //printf(_("Rate set to %iHz (requested %iHz)\n"), rrate, rate);
  /* set the buffer time */
  err = snd_pcm_hw_params_get_buffer_size_min(params, &buffer_size_min);
  err = snd_pcm_hw_params_get_buffer_size_max(params, &buffer_size_max);
  err = snd_pcm_hw_params_get_period_size_min(params, &period_size_min, NULL);
  err = snd_pcm_hw_params_get_period_size_max(params, &period_size_max, NULL);
  //printf(_("Buffer size range from %lu to %lu\n"),buffer_size_min, buffer_size_max);
  //printf(_("Period size range from %lu to %lu\n"),period_size_min, period_size_max);
  if (period_time > 0) {
    //printf(_("Requested period time %u us\n"), period_time);
    err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, NULL);
    if (err < 0) {
      fprintf(stderr, "Unable to set period time %u us for playback: %s\n",
         period_time, snd_strerror(err));
      return err;
    }
  }
  if (buffer_time > 0) {
    //printf(_("Requested buffer time %u us\n"), buffer_time);
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, NULL);
    if (err < 0) {
      fprintf(stderr, "Unable to set buffer time %u us for playback: %s\n",
         buffer_time, snd_strerror(err));
      return err;
    }
  }
  if (! buffer_time && ! period_time) {
    //buffer_size = buffer_size_max;
    //buffer_size = buffer_size_min;
    *buffer_size = 2048;
    if (! period_time)
      *buffer_size = (*buffer_size / nperiods) * nperiods;
    //printf(_("Using max buffer size %lu\n"), buffer_size);
    //printf(_("Using min buffer size %lu\n"), *buffer_size);
    err = snd_pcm_hw_params_set_buffer_size_near(handle, params, buffer_size);
    if (err < 0) {
      fprintf(stderr, "Unable to set buffer size %lu for playback: %s\n",
         *buffer_size, snd_strerror(err));
      return err;
    }
  }
  if (! buffer_time || ! period_time) {
    //printf(_("Periods = %u\n"), nperiods);
    err = snd_pcm_hw_params_set_periods_near(handle, params, &nperiods, NULL);
    if (err < 0) {
      fprintf(stderr, "Unable to set nperiods %u for playback: %s\n",
         nperiods, snd_strerror(err));
      return err;
    }
  }

  /* write the parameters to device */
  err = snd_pcm_hw_params(handle, params);
  if (err < 0) {
    fprintf(stderr, "Unable to set hw params for playback: %s\n", snd_strerror(err));
    return err;
  }

  snd_pcm_hw_params_get_buffer_size(params, buffer_size);
  snd_pcm_hw_params_get_period_size(params, period_size, NULL);
  //printf(_("was set period_size = %lu\n"),*period_size);
  //printf(_("was set buffer_size = %lu\n"),*buffer_size);
  if (2*(*period_size) > *buffer_size) {
    fprintf(stderr, "buffer to small, could not use\n");
    return -EINVAL;
  }

  return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams, snd_pcm_uframes_t *buffer_size, snd_pcm_uframes_t *period_size) {
  int err;

  /* get the current swparams */
  err = snd_pcm_sw_params_current(handle, swparams);
  if (err < 0) {
    fprintf(stderr, "Unable to determine current swparams for playback: %s\n", snd_strerror(err));
    return err;
  }

  /* start the transfer when a buffer is full */
  err = snd_pcm_sw_params_set_start_threshold(handle, swparams, *buffer_size - *period_size);
  if (err < 0) {
    fprintf(stderr, "Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
    return err;
  }

  /* allow the transfer when at least period_size frames can be processed */
  err = snd_pcm_sw_params_set_avail_min(handle, swparams, *period_size);
  if (err < 0) {
    fprintf(stderr, "Unable to set avail min for playback: %s\n", snd_strerror(err));
    return err;
  }

  /* write the parameters to the playback device */
  err = snd_pcm_sw_params(handle, swparams);
  if (err < 0) {
    fprintf(stderr, "Unable to set sw params for playback: %s\n", snd_strerror(err));
    return err;
  }

  return 0;
}

void *loop_beep(void *arg) {
  thread_vars_t *thread_vars = (thread_vars_t *)arg;
  uint8_t *err = &thread_vars->thread_err;

  while(*err == ERROR_NONE && *thread_vars->main_err_ptr == ERROR_NONE){

    if(thread_vars->config->beep_enabled){

      snd_pcm_t            *handle;
      int                   error;
      snd_pcm_hw_params_t  *hwparams;
      snd_pcm_sw_params_t  *swparams;

      snd_pcm_hw_params_alloca(&hwparams);
      snd_pcm_sw_params_alloca(&swparams);

      char              *device      = "default";       /* playback device */
      snd_pcm_format_t   format      = SND_PCM_FORMAT_S16; /* sample format */
      unsigned int       rate        = 48000;              /* stream rate */
      unsigned int       channels    = 2;              /* count of channels */
      snd_pcm_uframes_t  buffer_size;
      snd_pcm_uframes_t  period_size;

      if ((error = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
          printf("Playback open error: %d,%s\n", error,snd_strerror(error));
          *err = ERROR_THREAD_ERROR;
          return NULL;
      }

      if ((error = set_hwparams(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED, format, rate, channels, &buffer_size, &period_size)) < 0) {
          printf("Setting of hwparams failed: %s\n", snd_strerror(error));
          snd_pcm_close(handle);
          *err = ERROR_THREAD_ERROR;
          return NULL;
      }
      if ((error = set_swparams(handle, swparams, &buffer_size, &period_size)) < 0) {
          printf("Setting of swparams failed: %s\n", snd_strerror(error));
          snd_pcm_close(handle);
          *err = ERROR_THREAD_ERROR;
          return NULL;
      }

      uint8_t              *frames;
      frames = malloc(snd_pcm_frames_to_bytes(handle, period_size));

      if (frames == NULL) {
          fprintf(stderr, "No enough memory\n");
          *err = ERROR_THREAD_ERROR;
          return NULL;
      }

      snd_pcm_prepare(handle);

      double freq = 400.0;
      double phase  = 0.0;

      if(thread_vars->status->modulation_error_rate > 0 && thread_vars->status->modulation_error_rate <= 310)
      {
        freq = 700.0 * (exp((200+(10*thread_vars->status->modulation_error_rate))/1127.0)-1.0);
      }
      generate_sine(frames, period_size, &phase, freq, rate, channels, (thread_vars->status->state == STATE_DEMOD_S2));

      /* Start playback */
      snd_pcm_writei(handle, frames, period_size);
      snd_pcm_start(handle);

      snd_pcm_sframes_t avail;

      while(*err == ERROR_NONE && *thread_vars->main_err_ptr == ERROR_NONE && thread_vars->config->beep_enabled){
          /* Wait until device is ready for more data */
          if(snd_pcm_wait(handle, 100))
          {
              avail = snd_pcm_avail_update(handle);
              if(avail < 0 && avail == -32)
              {
                /* Handle underrun */
                snd_pcm_prepare(handle);
              }
              while (avail >= (snd_pcm_sframes_t)period_size)
              {
                if(thread_vars->status->modulation_error_rate > 0 && thread_vars->status->modulation_error_rate <= 310)
                {
                  freq = 700.0 * (exp((200+(10*thread_vars->status->modulation_error_rate))/1127.0)-1.0);
                }

                  generate_sine(frames, period_size, &phase, freq, rate, channels, (thread_vars->status->state == STATE_DEMOD_S2));
                  while (snd_pcm_writei(handle, frames, period_size) < 0)
                  {
                      /* Handle underrun */
                      snd_pcm_prepare(handle);
                  }
                  avail = snd_pcm_avail_update(handle);
              }
          }
      }

      /* Stop Playback */
      snd_pcm_drop(handle);

      /* Empty buffer */
      snd_pcm_drain(handle);

      free(frames);
      snd_pcm_close(handle);
    }

    usleep(10*1000); // 10ms

  }

  return NULL;
}