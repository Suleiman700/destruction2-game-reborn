// Replacement for the DirectX SDK wavread.cpp (mmio-based). Keeps the exact chunk semantics the
// sound engine depends on: m_ckInRiff.cksize is the RIFF chunk size and m_ckIn.cksize counts down
// the remaining bytes of the 'data' chunk.
#include <windows.h>
#include <mmsystem.h>
#include <string>
#include <fstream.h>
#include "wavread.h"

struct D2WaveFile {
    std::string bytes;
    size_t riffDataOffset;  // offset of the RIFF form type ("WAVE")
    size_t readPos;
};

static DWORD rd32(const std::string &b, size_t o) {
    return (BYTE)b[o] | ((BYTE)b[o + 1] << 8) | ((BYTE)b[o + 2] << 16) | ((DWORD)(BYTE)b[o + 3] << 24);
}

CWaveSoundRead::CWaveSoundRead() { m_pwfx = NULL; m_hmmioIn = NULL; }
CWaveSoundRead::~CWaveSoundRead() {
    Close();
    if (m_pwfx) { delete[] (BYTE *)m_pwfx; m_pwfx = NULL; }
}

HRESULT CWaveSoundRead::Open(CHAR *strFilename) {
    if (m_pwfx) { delete[] (BYTE *)m_pwfx; m_pwfx = NULL; }
    Close();
    D2WaveFile *wf = new D2WaveFile();
    if (!d2io::read_file(strFilename, wf->bytes) || wf->bytes.size() < 12 ||
        wf->bytes.compare(0, 4, "RIFF") != 0 || wf->bytes.compare(8, 4, "WAVE") != 0) {
        delete wf;
        return E_FAIL;
    }
    const std::string &b = wf->bytes;
    m_ckInRiff.ckid = FOURCC_RIFF;
    m_ckInRiff.cksize = rd32(b, 4);
    m_ckInRiff.fccType = mmioFOURCC('W', 'A', 'V', 'E');
    m_ckInRiff.dwDataOffset = 8;
    wf->riffDataOffset = 8;
    // find 'fmt '
    size_t p = 12;
    bool found = false;
    while (p + 8 <= b.size()) {
        DWORD sz = rd32(b, p + 4);
        if (b.compare(p, 4, "fmt ") == 0) {
            if (sz < sizeof(PCMWAVEFORMAT)) { delete wf; return E_FAIL; }
            PCMWAVEFORMAT pcm;
            memcpy(&pcm, b.data() + p + 8, sizeof(pcm));
            WORD extra = 0;
            if (pcm.wf.wFormatTag != WAVE_FORMAT_PCM && sz >= sizeof(PCMWAVEFORMAT) + 2)
                memcpy(&extra, b.data() + p + 8 + sizeof(pcm), 2);
            m_pwfx = (WAVEFORMATEX *)new BYTE[sizeof(WAVEFORMATEX) + extra];
            memcpy(m_pwfx, &pcm, sizeof(pcm));
            m_pwfx->cbSize = extra;
            found = true;
            break;
        }
        p += 8 + sz + (sz & 1);
    }
    if (!found) { delete wf; return E_FAIL; }
    m_hmmioIn = (HMMIO)wf;
    return Reset();
}

HRESULT CWaveSoundRead::Reset() {
    D2WaveFile *wf = (D2WaveFile *)m_hmmioIn;
    if (!wf) return E_FAIL;
    const std::string &b = wf->bytes;
    size_t p = wf->riffDataOffset + 4;
    while (p + 8 <= b.size()) {
        DWORD sz = rd32(b, p + 4);
        if (b.compare(p, 4, "data") == 0) {
            m_ckIn.ckid = mmioFOURCC('d', 'a', 't', 'a');
            m_ckIn.cksize = sz;
            m_ckIn.dwDataOffset = (DWORD)(p + 8);
            wf->readPos = p + 8;
            return S_OK;
        }
        p += 8 + sz + (sz & 1);
    }
    return E_FAIL;
}

HRESULT CWaveSoundRead::Read(UINT nSizeToRead, BYTE *pbData, UINT *pnSizeRead) {
    D2WaveFile *wf = (D2WaveFile *)m_hmmioIn;
    *pnSizeRead = 0;
    if (!wf) return E_FAIL;
    UINT n = nSizeToRead;
    if (n > m_ckIn.cksize) n = m_ckIn.cksize;
    if (wf->readPos + n > wf->bytes.size()) return E_FAIL;  // truncated file: mmioAdvance would fail
    m_ckIn.cksize -= n;
    memcpy(pbData, wf->bytes.data() + wf->readPos, n);
    wf->readPos += n;
    *pnSizeRead = n;
    return S_OK;
}

HRESULT CWaveSoundRead::Close() {
    delete (D2WaveFile *)m_hmmioIn;
    m_hmmioIn = NULL;
    return S_OK;
}
