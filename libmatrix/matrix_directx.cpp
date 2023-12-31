/*****************************************************************************
 * directx.c: Windows DirectX audio output method
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 * $Id: 2e02277ca52dea61024b60dbd9fe256725e35dca $
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include "outMixer.h"
#define restrict __restrict

#include "windows_audio_common.h"

#define DS_BUF_SIZE (1024*1024)

/*****************************************************************************
 * aout_sys_t: directx audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the direct sound specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    HINSTANCE           hdsound_dll;  /* handle of the opened dsound dll */

    LPDIRECTSOUND       p_dsobject;   /* main Direct Sound object */
    LPDIRECTSOUNDBUFFER p_dsbuffer;   /* the sound buffer we use (direct sound
                                       * takes care of mixing all the
                                       * secondary buffers into the primary) */

    LPDIRECTSOUNDNOTIFY p_notify;
    HANDLE hnotify_evt;
    struct
    {
        float            volume;
        LONG             mb;
        bool             mute;
    } volume;

    int      i_bytes_per_sample;      /* Size in bytes of one frame */     
    int      i_rate;                  /* Sample rate */ 

    uint8_t  chans_to_reorder;        /* do we need channel reordering */
    uint8_t  chan_table[AOUT_CHAN_MAX];
    uint32_t i_channel_mask;
    vlc_fourcc_t format;

    size_t  i_write;

    outMixer *p_mixer;
    DWORD dwOffset;
};

outMixer *g_mixer = NULL;
extern "C" __declspec( dllexport ) void Config(HWND hwndParent)
{
    if (g_mixer)
        g_mixer->Config(hwndParent);
}

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );
static void Stop( audio_output_t * );
static void Play( audio_output_t *, block_t * );
static int  VolumeSet( audio_output_t *, float );
static int  MuteSet( audio_output_t *, bool );
static void Flush( audio_output_t *, bool );
static void Pause( audio_output_t *, bool, mtime_t );
static int  TimeGet( audio_output_t *, mtime_t *);

/* local functions */
static int  InitDirectSound   ( audio_output_t * );
static int  CreateDSBuffer    ( audio_output_t *, int, int, int, int, bool );
static int  CreateDSBufferPCM ( audio_output_t *, vlc_fourcc_t*, int, int, bool );
static void DestroyDSBuffer   ( audio_output_t * );
static int  FillBuffer        ( audio_output_t *, block_t * );

static int ConfigDevicesCallback( vlc_object_t *, const char *, char ***, char *** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define VOLUME_TEXT N_("Audio volume")
#define VOLUME_LONGTEXT N_("Audio volume in hundredths of decibels (dB).")

vlc_module_begin ()
    set_description( N_("MatrixMixer audio output") )
    set_shortname( "Matrix" )
    set_capability( "audio output", 100 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_shortcut( "matrix", "matrixmixer" )

    add_string( "matrix_directx_dummy", "default", N_(" "), N_(" "), false )
        change_string_cb( ConfigDevicesCallback )

    set_callbacks( Open, Close )
vlc_module_end ()

/*
output format
nb channels + disposition
samplerate

aout_FormatNbChannels => use p_aout->output.output.i_physical_channels to get nb of channels
aout_VolumeSoftInit => init volume callbacks
aout_FormatPrepare => set i_channels, i_bitspersample, i_bytes_per_frame, i_frame_length=1 based on p_aout->output.output.i_format

i_format				fourCC
i_physical_channels		L+R+C+RL+RR+LFE
i_channels				6
i_bitspersample			16
i_bytes_per_frame		i_bitspersample/8*channels => 16/8*6=12
i_nb_samples			rate/20 => 48000/20=2400
i_frame_length			1


OpenAudio
	=> init
	=> load DSound
	=> create event
CloseAudio
	=> free event
	=> close DSound

Start
	=> InitDirectSound
	=> check devices capabilities
	=> CreateDSBuffer / CreateDSBufferPCM
Stop
	=> stop DSound
	=> release DSound
Play
	=> FillBuffer
	=> dsound play
FillBuffer
	=> lock DS Buffer
	=> write data
	=> unlock DS Buffer

InitDirectSound			=> DirectSound lifecycle
CreateDSBuffer			=> DirectSound lifecycle
CreateDSBufferPCM		=> DirectSound lifecycle
DestroyDSBuffer			=> DirectSound lifecycle
*/

/*****************************************************************************
 * OpenAudio: open the audio device
 *****************************************************************************
 * This function opens and setups Direct Sound.
 *****************************************************************************/
static int Start( audio_output_t *p_aout, audio_sample_format_t *restrict fmt )
{
    int i = 0;

    msg_Dbg( p_aout, "Opening DirectSound Audio Output" );

    /* Initialise DirectSound */
    if( InitDirectSound( p_aout ) )
    {
        msg_Err( p_aout, "cannot initialize DirectSound" );
        goto error;
    }

#if 0
    if( i == 0 )
    {
        DWORD ui_speaker_config;

        /* Check the speaker configuration to determine which channel config
         * should be the default */
        if( FAILED( IDirectSound_GetSpeakerConfig( p_aout->sys->p_dsobject,
                                              &ui_speaker_config ) ) )
        {
            ui_speaker_config = DSSPEAKER_STEREO;
            msg_Dbg( p_aout, "GetSpeakerConfig failed" );
        }
        fmt->i_physical_channels = AOUT_CHANS_2_0;

        const char *name = "Unknown";
        switch( DSSPEAKER_CONFIG(ui_speaker_config) )
        {
        case DSSPEAKER_7POINT1:
        case DSSPEAKER_7POINT1_SURROUND:
            name = "7.1";
            fmt->i_physical_channels = AOUT_CHANS_7_1;
            break;
        case DSSPEAKER_5POINT1:
        case DSSPEAKER_5POINT1_SURROUND:
            name = "5.1";
            fmt->i_physical_channels = AOUT_CHANS_5_1;
            break;
        case DSSPEAKER_QUAD:
            name = "Quad";
            fmt->i_physical_channels = AOUT_CHANS_4_0;
            break;
#if 0 /* Lots of people just get their settings wrong and complain that
       * this is a problem with VLC so just don't ever set mono by default. */
        case DSSPEAKER_MONO:
            name = "Mono";
            fmt->i_physical_channels = AOUT_CHAN_CENTER;
            break;
#endif
        case DSSPEAKER_SURROUND: /* XXX: stereo, really? -- Courmisch */
            name = "Surround";
            break;
        case DSSPEAKER_STEREO:
            name = "Stereo";
            break;
        }
        msg_Dbg( p_aout, "%s speaker config: %s", "Windows", name );
    }
    else
    {   /* Overriden speaker configuration */
        const char *name = "Non-existant";
        switch( i )
        {
        case 1: /* Mono */
            name = "Mono";
            fmt->i_physical_channels = AOUT_CHAN_CENTER;
            break;
        case 2: /* Stereo */
            name = "Stereo";
            fmt->i_physical_channels = AOUT_CHANS_2_0;
            break;
        case 3: /* Quad */
            name = "Quad";
            fmt->i_physical_channels = AOUT_CHANS_4_0;
            break;
        case 4: /* 5.1 */
            name = "5.1";
            fmt->i_physical_channels = AOUT_CHANS_5_1;
            break;
        case 5: /* 7.1 */
            name = "7.1";
            fmt->i_physical_channels = AOUT_CHANS_7_1;
            break;
        }
        msg_Dbg( p_aout, "%s speaker config: %s", "VLC", name );
    }
#endif

    p_aout->sys->p_mixer = new outMixer(fmt->i_format, fmt->i_physical_channels, fmt->i_rate);
    g_mixer = p_aout->sys->p_mixer;

    /* Open the device */
    {
        aout_FormatPrepare( fmt );

        if( CreateDSBufferPCM( p_aout, &fmt->i_format,
                               g_mixer->getChannels(), //fmt->i_physical_channels,
                               g_mixer->getRate(), //fmt->i_rate,
                               false )
            != VLC_SUCCESS )
        {
            msg_Err( p_aout, "cannot open directx audio device" );
            goto error;
        }
    }
    p_aout->sys->i_write = 0;
    fmt->i_format = VLC_CODEC_FL32;
    p_aout->sys->dwOffset = 0;

    /* Force volume update */
    VolumeSet( p_aout, p_aout->sys->volume.volume );
    MuteSet( p_aout, p_aout->sys->volume.mute );

    /* then launch the notification thread */
    p_aout->time_get = TimeGet;
    p_aout->play = Play;
    p_aout->pause = Pause;
    p_aout->flush = Flush;

    return VLC_SUCCESS;

 error:
    Stop( p_aout );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Play: we'll start playing the directsound buffer here because at least here
 *       we know the first buffer has been put in the aout fifo and we also
 *       know its date.
 *****************************************************************************/
static void Play( audio_output_t *p_aout, block_t *p_buffer )
{
    if(  FillBuffer( p_aout, p_buffer ) ==  VLC_SUCCESS )
    {
        /* start playing the buffer */
        HRESULT dsresult = IDirectSoundBuffer_Play( p_aout->sys->p_dsbuffer,
                                                    0, 0, DSBPLAY_LOOPING );
        if( dsresult == DSERR_BUFFERLOST )
        {
            IDirectSoundBuffer_Restore( p_aout->sys->p_dsbuffer );
            dsresult = IDirectSoundBuffer_Play( p_aout->sys->p_dsbuffer,
                                                0, 0, DSBPLAY_LOOPING );
        }
        if( dsresult != DS_OK )
            msg_Err( p_aout, "cannot start playing buffer" );
    }
}

static int VolumeSet( audio_output_t *p_aout, float volume )
{
    aout_sys_t *sys = p_aout->sys;
    int ret = 0;

    /* Convert UI volume to linear factor (cube) */
    float vol = volume * volume * volume;

    /* millibels from linear amplification */
    LONG mb = lroundf(2000.f * log10f(vol));

    /* Clamp to allowed DirectSound range */
    //static_assert( DSBVOLUME_MIN < DSBVOLUME_MAX, "DSBVOLUME_* confused" );
    if( mb > DSBVOLUME_MAX )
    {
        mb = DSBVOLUME_MAX;
        ret = -1;
    }
    if( mb <= DSBVOLUME_MIN )
        mb = DSBVOLUME_MIN;

    sys->volume.mb = mb;
    sys->volume.volume = volume;
    if( !sys->volume.mute && sys->p_dsbuffer &&
        IDirectSoundBuffer_SetVolume( sys->p_dsbuffer, mb ) != DS_OK )
        return -1;
    /* Convert back to UI volume */
    aout_VolumeReport( p_aout, volume );

    if( var_InheritBool( p_aout, "volume-save" ) )
        config_PutFloat( p_aout, "directx-volume", volume );
    return ret;
}

static int MuteSet( audio_output_t *p_aout, bool mute )
{
    HRESULT res = DS_OK;
    aout_sys_t *sys = p_aout->sys;

    sys->volume.mute = mute;

    if( sys->p_dsbuffer )
        res = IDirectSoundBuffer_SetVolume( sys->p_dsbuffer,
                                            mute? DSBVOLUME_MIN : sys->volume.mb );

    aout_MuteReport( p_aout, mute );
    return (res != DS_OK);
}

/*****************************************************************************
 * CloseAudio: close the audio device
 *****************************************************************************/
static void Stop( audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;
    msg_Dbg( p_aout, "closing audio device" );

    if( p_sys->p_notify )
        IDirectSoundNotify_Release(p_sys->p_notify );
    p_sys->p_notify = NULL;

    IDirectSoundBuffer_Stop( p_sys->p_dsbuffer );
    /* release the secondary buffer */
    DestroyDSBuffer( p_aout );

    /* finally release the DirectSound object */
    if( p_sys->p_dsobject )
        IDirectSound_Release( p_sys->p_dsobject );

    delete p_sys->p_mixer;
    g_mixer = NULL;
    p_aout->sys->dwOffset = 0;
}

/*****************************************************************************
 * InitDirectSound: handle all the gory details of DirectSound initialisation
 *****************************************************************************/
static int InitDirectSound( audio_output_t *p_aout )
{
    aout_sys_t *sys = p_aout->sys;
    GUID guid, *p_guid = NULL;
    HRESULT (WINAPI *OurDirectSoundCreate)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);

    OurDirectSoundCreate = (HRESULT (WINAPI *)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN)) GetProcAddress( p_aout->sys->hdsound_dll, "DirectSoundCreate" );
    if( OurDirectSoundCreate == NULL )
    {
        msg_Warn( p_aout, "GetProcAddress FAILED" );
        goto error;
    }

    /* Create the direct sound object */
    if FAILED( OurDirectSoundCreate( p_guid, &sys->p_dsobject, NULL ) )
    {
        msg_Warn( p_aout, "cannot create a direct sound device" );
        goto error;
    }

    /* Set DirectSound Cooperative level, ie what control we want over Windows
     * sound device. In our case, DSSCL_EXCLUSIVE means that we can modify the
     * settings of the primary buffer, but also that only the sound of our
     * application will be hearable when it will have the focus.
     * !!! (this is not really working as intended yet because to set the
     * cooperative level you need the window handle of your application, and
     * I don't know of any easy way to get it. Especially since we might play
     * sound without any video, and so what window handle should we use ???
     * The hack for now is to use the Desktop window handle - it seems to be
     * working */
#if !VLC_WINSTORE_APP
    if( IDirectSound_SetCooperativeLevel( p_aout->sys->p_dsobject,
                                          GetDesktopWindow(),
                                          DSSCL_EXCLUSIVE) )
    {
        msg_Warn( p_aout, "cannot set direct sound cooperative level" );
    }
#endif
    return VLC_SUCCESS;

 error:
    sys->p_dsobject = NULL;
    return VLC_EGENERIC;

}

/*****************************************************************************
 * CreateDSBuffer: Creates a direct sound buffer of the required format.
 *****************************************************************************
 * This function creates the buffer we'll use to play audio.
 * In DirectSound there are two kinds of buffers:
 * - the primary buffer: which is the actual buffer that the soundcard plays
 * - the secondary buffer(s): these buffers are the one actually used by
 *    applications and DirectSound takes care of mixing them into the primary.
 *
 * Once you create a secondary buffer, you cannot change its format anymore so
 * you have to release the current one and create another.
 *****************************************************************************/
static int CreateDSBuffer( audio_output_t *p_aout, int i_format,
                           int i_channels, int i_nb_channels, int i_rate,
                           bool b_probe )
{
    WAVEFORMATEXTENSIBLE waveformat;
    DSBUFFERDESC         dsbdesc;

    /* First set the sound buffer format */
    waveformat.dwChannelMask = 0;
    for( unsigned i = 0; pi_vlc_chan_order_wg4[i]; i++ )
        if( i_channels & pi_vlc_chan_order_wg4[i] )
            waveformat.dwChannelMask |= pi_channels_in[i];

    switch( i_format )
    {
    case VLC_CODEC_SPDIFL:
        i_nb_channels = 2;
        /* To prevent channel re-ordering */
        waveformat.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
        waveformat.Format.wBitsPerSample = 16;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_DOLBY_AC3_SPDIF;
        waveformat.SubFormat = _KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF;
        break;

    case VLC_CODEC_FL32:
        waveformat.Format.wBitsPerSample = sizeof(float) * 8;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        waveformat.SubFormat = _KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        break;

    case VLC_CODEC_S16N:
        waveformat.Format.wBitsPerSample = 16;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_PCM;
        waveformat.SubFormat = _KSDATAFORMAT_SUBTYPE_PCM;
        break;
    }

    waveformat.Format.nChannels = i_nb_channels;
    waveformat.Format.nSamplesPerSec = i_rate;
    waveformat.Format.nBlockAlign =
        waveformat.Format.wBitsPerSample / 8 * i_nb_channels;
    waveformat.Format.nAvgBytesPerSec =
        waveformat.Format.nSamplesPerSec * waveformat.Format.nBlockAlign;

    p_aout->sys->i_bytes_per_sample = waveformat.Format.nBlockAlign;
    p_aout->sys->format = i_format;

    /* Then fill in the direct sound descriptor */
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 /* Better position accuracy */
                    | DSBCAPS_GLOBALFOCUS         /* Allows background playing */
                    | DSBCAPS_CTRLVOLUME          /* Allows volume control */
                    | DSBCAPS_CTRLPOSITIONNOTIFY; /* Allow position notifications */

    /* Only use the new WAVE_FORMAT_EXTENSIBLE format for multichannel audio */
    if( i_nb_channels <= 2 )
    {
        waveformat.Format.cbSize = 0;
    }
    else
    {
        waveformat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        waveformat.Format.cbSize =
            sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

        /* Needed for 5.1 on emu101k */
        dsbdesc.dwFlags |= DSBCAPS_LOCHARDWARE;
    }

    dsbdesc.dwBufferBytes = DS_BUF_SIZE; /* buffer size */
    dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&waveformat;

    if FAILED( IDirectSound_CreateSoundBuffer(
                   p_aout->sys->p_dsobject, &dsbdesc,
                   &p_aout->sys->p_dsbuffer, NULL) )
    {
        if( dsbdesc.dwFlags & DSBCAPS_LOCHARDWARE )
        {
            /* Try without DSBCAPS_LOCHARDWARE */
            dsbdesc.dwFlags &= ~DSBCAPS_LOCHARDWARE;
            if FAILED( IDirectSound_CreateSoundBuffer(
                   p_aout->sys->p_dsobject, &dsbdesc,
                   &p_aout->sys->p_dsbuffer, NULL) )
            {
                return VLC_EGENERIC;
            }
            if( !b_probe )
                msg_Dbg( p_aout, "couldn't use hardware sound buffer" );
        }
        else
        {
            return VLC_EGENERIC;
        }
    }

    /* Stop here if we were just probing */
    if( b_probe )
    {
        IDirectSoundBuffer_Release( p_aout->sys->p_dsbuffer );
        p_aout->sys->p_dsbuffer = NULL;
        return VLC_SUCCESS;
    }

    p_aout->sys->i_rate = i_rate;
    p_aout->sys->i_channel_mask = waveformat.dwChannelMask;
    p_aout->sys->chans_to_reorder =
        aout_CheckChannelReorder( pi_channels_in, pi_channels_out,
                                  waveformat.dwChannelMask,
                                  p_aout->sys->chan_table );
    if( p_aout->sys->chans_to_reorder )
    {
        msg_Dbg( p_aout, "channel reordering needed" );
    }

    if( IDirectSoundBuffer_QueryInterface( p_aout->sys->p_dsbuffer,
                                           IID_IDirectSoundNotify,
                                           (void **) &p_aout->sys->p_notify )
            != DS_OK )
    {
        
        msg_Err(p_aout, "Couldn't query IDirectSoundNotify");
        p_aout->sys->p_notify = NULL;
    }

    FillBuffer(p_aout,NULL);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CreateDSBufferPCM: creates a PCM direct sound buffer.
 *****************************************************************************
 * We first try to create a WAVE_FORMAT_IEEE_FLOAT buffer if supported by
 * the hardware, otherwise we create a WAVE_FORMAT_PCM buffer.
 ****************************************************************************/
static int CreateDSBufferPCM( audio_output_t *p_aout, vlc_fourcc_t *i_format,
                              int i_channels, int i_rate, bool b_probe )
{
    unsigned i_nb_channels = popcount( i_channels );

    if ( CreateDSBuffer( p_aout, VLC_CODEC_S16N,
                         i_channels, i_nb_channels, i_rate, b_probe )
         != VLC_SUCCESS )
    {
        return VLC_EGENERIC;
    }
    else
    {
        *i_format = VLC_CODEC_S16N;
        return VLC_SUCCESS;
    }
}

/*****************************************************************************
 * DestroyDSBuffer
 *****************************************************************************
 * This function destroys the secondary buffer.
 *****************************************************************************/
static void DestroyDSBuffer( audio_output_t *p_aout )
{
    if( p_aout->sys->p_dsbuffer )
    {
        IDirectSoundBuffer_Release( p_aout->sys->p_dsbuffer );
        p_aout->sys->p_dsbuffer = NULL;
    }
}

/*****************************************************************************
 * FillBuffer: Fill in one of the direct sound frame buffers.
 *****************************************************************************
 * Returns VLC_SUCCESS on success.
 *****************************************************************************/
static int FillBuffer( audio_output_t *p_aout, block_t *p_buffer )
{
    aout_sys_t *p_sys = p_aout->sys;

    size_t towrite = (p_buffer)?p_buffer->i_buffer:DS_BUF_SIZE;
    void *p_write_position, *p_wrap_around;
    unsigned long l_bytes1, l_bytes2;
    HRESULT dsresult;

    // traitement du signal par valib
    p_sys->p_mixer->Process(p_buffer ? p_buffer->p_buffer : 0, towrite);

    // lecture du signal modifi�
    Chunk *chunk = NULL;
    while ((chunk = p_sys->p_mixer->getChunk()) != NULL)
    {
        /* Before copying anything, we have to lock the buffer */
        dsresult = IDirectSoundBuffer_Lock(
                    p_sys->p_dsbuffer,         /* DS buffer */
                    p_sys->dwOffset,           /* Start offset */
                    chunk->size,               /* Number of bytes */
                    &p_write_position,         /* Address of lock start */
                    &l_bytes1,                 /* Count of bytes locked before wrap around */
                    &p_wrap_around,            /* Buffer address (if wrap around) */
                    &l_bytes2,                 /* Count of bytes after wrap around */
                    0 );                       /* Flags: DSBLOCK_FROMWRITECURSOR is buggy */
        if( dsresult == DSERR_BUFFERLOST )
        {
            IDirectSoundBuffer_Restore( p_sys->p_dsbuffer );
            dsresult = IDirectSoundBuffer_Lock(
                                   p_sys->p_dsbuffer,
                                   p_sys->dwOffset, //p_aout->sys->i_write,
                                   chunk->size, //towrite,
                                   &p_write_position,
                                   &l_bytes1,
                                   &p_wrap_around,
                                   &l_bytes2,
                                   0 );
        }
        if( dsresult != DS_OK )
        {
            msg_Warn( p_aout, "cannot lock buffer" );
            if( p_buffer ) block_Release( p_buffer );
            return VLC_EGENERIC;
        }

        if( l_bytes2 == 0 )
        {
            memcpy( p_write_position, chunk->rawdata, l_bytes1 );
            p_sys->dwOffset += l_bytes1;
        }
        else
        {
            memcpy( p_write_position, chunk->rawdata, l_bytes1 );
            memcpy( p_wrap_around, chunk->rawdata+l_bytes1, l_bytes2 );
            p_sys->dwOffset = l_bytes2;
        }
        if (p_sys->dwOffset >= (DWORD)DS_BUF_SIZE)
            p_sys->dwOffset = 0;
        p_sys->i_write += chunk->size;
        p_sys->i_write %= DS_BUF_SIZE;

        /* Now the data has been copied, unlock the buffer */
        IDirectSoundBuffer_Unlock( p_sys->p_dsbuffer, p_write_position, l_bytes1,
                                   p_wrap_around, l_bytes2 );
    }

    if (p_buffer) block_Release( p_buffer );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ConfigDevicesCallback
 *****************************************************************************/
static int ConfigDevicesCallback( vlc_object_t *p_this, char const *psz_name,
                                  char ***values, char ***descs )
{
    Config(GetForegroundWindow());
    return 0;
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = (aout_sys_t *) calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->hdsound_dll = LoadLibrary(_T("DSOUND.DLL"));
    if (sys->hdsound_dll == NULL)
    {
        msg_Warn(aout, "cannot open DSOUND.DLL");
        free(sys);
        return VLC_EGENERIC;
    }

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout->volume_set = VolumeSet;
    aout->mute_set = MuteSet;
    aout->device_select = NULL;

    /* Volume */
    sys->volume.volume = var_InheritFloat(aout, "directx-volume");
    aout_VolumeReport(aout, sys->volume.volume );
    MuteSet(aout, var_InheritBool(aout, "mute"));

    sys->hnotify_evt = CreateEvent(NULL, FALSE, TRUE, NULL);
    if( !sys->hnotify_evt )
    {
        msg_Err(aout, "cannot create Event");
        FreeLibrary(sys->hdsound_dll);
        free(sys);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    CloseHandle(sys->hnotify_evt);
    FreeLibrary(sys->hdsound_dll); /* free DSOUND.DLL */
    free(sys);
}

static void Flush ( audio_output_t * aout, bool drain )
{
    aout_sys_t *sys = aout->sys;
    size_t read;
    if( drain )
    {
        if( sys->p_notify )
        {
            DSBPOSITIONNOTIFY notif;
            notif.dwOffset = aout->sys->i_write;
            notif.hEventNotify = sys->hnotify_evt;
            if( IDirectSoundNotify_SetNotificationPositions( sys->p_notify, 1, &notif ) ==  DS_OK )
            {
                WaitForSingleObject( sys->hnotify_evt, INFINITE );
                IDirectSoundBuffer_Stop( aout->sys->p_dsbuffer );
            }
        }
        else
            while( IDirectSoundBuffer_GetCurrentPosition( aout->sys->p_dsbuffer,(LPDWORD) &read, NULL) ==  DS_OK )
            {
                read %= DS_BUF_SIZE;
                if( read == aout->sys->i_write )
                    break;
                msleep(10000);
            }
    }
    else
    {
        IDirectSoundBuffer_Stop( aout->sys->p_dsbuffer );
        IDirectSoundBuffer_SetCurrentPosition( aout->sys->p_dsbuffer, 
                                               aout->sys->i_write );
    }
}

static void Pause( audio_output_t * aout, bool pause, mtime_t date )
{
    (void) date;
    if( pause )
        IDirectSoundBuffer_Stop( aout->sys->p_dsbuffer );
    else
        IDirectSoundBuffer_Play( aout->sys->p_dsbuffer, 0, 0, DSBPLAY_LOOPING );
}


static int TimeGet( audio_output_t * aout, mtime_t * delay )
{
    uint32_t read;
    mtime_t size;

    if( IDirectSoundBuffer_GetCurrentPosition( aout->sys->p_dsbuffer, (LPDWORD) &read, NULL) != DS_OK )
        return 1;

    read %= DS_BUF_SIZE;

    size = (mtime_t)aout->sys->i_write - (mtime_t) read;
    if( size < 0 )
        size += DS_BUF_SIZE;

    *delay = ( size / aout->sys->i_bytes_per_sample ) * CLOCK_FREQ / aout->sys->i_rate;
    return 0;
}
