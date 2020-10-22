// Copyright 2015-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdlib.h>
#include <string.h>
#include <sys/param.h> // For MIN/MAX
#include "spi_flash_chip_generic.h"
#include "spi_flash_defs.h"
#include "esp_log.h"
#include "esp_attr.h"


static const char TAG[] = "chip_generic";


typedef struct flash_chip_dummy {
    uint8_t dio_dummy_bitlen;
    uint8_t qio_dummy_bitlen;
    uint8_t qout_dummy_bitlen;
    uint8_t dout_dummy_bitlen;
    uint8_t fastrd_dummy_bitlen;
    uint8_t slowrd_dummy_bitlen;
} flash_chip_dummy_t;

// These parameters can be placed in the ROM. For now we use the code in IDF.
DRAM_ATTR const static flash_chip_dummy_t default_flash_chip_dummy = {
    .dio_dummy_bitlen = SPI_FLASH_DIO_DUMMY_BITLEN,
    .qio_dummy_bitlen = SPI_FLASH_QIO_DUMMY_BITLEN,
    .qout_dummy_bitlen = SPI_FLASH_QOUT_DUMMY_BITLEN,
    .dout_dummy_bitlen = SPI_FLASH_DOUT_DUMMY_BITLEN,
    .fastrd_dummy_bitlen = SPI_FLASH_FASTRD_DUMMY_BITLEN,
    .slowrd_dummy_bitlen = SPI_FLASH_SLOWRD_DUMMY_BITLEN,
};

DRAM_ATTR flash_chip_dummy_t *rom_flash_chip_dummy = (flash_chip_dummy_t *)&default_flash_chip_dummy;

#define SPI_FLASH_DEFAULT_IDLE_TIMEOUT_MS           200
#define SPI_FLASH_GENERIC_CHIP_ERASE_TIMEOUT_MS     4000
#define SPI_FLASH_GENERIC_SECTOR_ERASE_TIMEOUT_MS   500  //according to GD25Q127 + 100ms
#define SPI_FLASH_GENERIC_BLOCK_ERASE_TIMEOUT_MS    1300  //according to GD25Q127 + 100ms
#define SPI_FLASH_GENERIC_PAGE_PROGRAM_TIMEOUT_MS   500

#define HOST_DELAY_INTERVAL_US                      1
#define CHIP_WAIT_IDLE_INTERVAL_US                  20

const DRAM_ATTR flash_chip_op_timeout_t spi_flash_chip_generic_timeout = {
    .chip_erase_timeout = SPI_FLASH_GENERIC_CHIP_ERASE_TIMEOUT_MS * 1000,
    .block_erase_timeout = SPI_FLASH_GENERIC_BLOCK_ERASE_TIMEOUT_MS * 1000,
    .sector_erase_timeout = SPI_FLASH_GENERIC_SECTOR_ERASE_TIMEOUT_MS * 1000,
    .idle_timeout = SPI_FLASH_DEFAULT_IDLE_TIMEOUT_MS * 1000,
    .page_program_timeout = SPI_FLASH_GENERIC_PAGE_PROGRAM_TIMEOUT_MS * 1000,
};

esp_err_t spi_flash_chip_generic_probe(esp_flash_t *chip, uint32_t flash_id)
{
    // This is the catch-all probe function, claim the chip always if nothing
    // else has claimed it yet.
    return ESP_OK;
}

esp_err_t spi_flash_chip_generic_reset(esp_flash_t *chip)
{
    //this is written following the winbond spec..
    spi_flash_trans_t t;
    t = (spi_flash_trans_t) {
        .command = CMD_RST_EN,
    };
    esp_err_t err = chip->host->driver->common_command(chip->host, &t);
    if (err != ESP_OK) {
        return err;
    }

    t = (spi_flash_trans_t) {
        .command = CMD_RST_DEV,
    };
    err = chip->host->driver->common_command(chip->host, &t);
    if (err != ESP_OK) {
        return err;
    }

    err = chip->chip_drv->wait_idle(chip, chip->chip_drv->timeout->idle_timeout);
    return err;
}

esp_err_t spi_flash_chip_generic_detect_size(esp_flash_t *chip, uint32_t *size)
{
    uint32_t id = chip->chip_id;
    *size = 0;

    /* Can't detect size unless the high byte of the product ID matches the same convention, which is usually 0x40 or
     * 0xC0 or similar. */
    if (((id & 0xFFFF) == 0x0000) || ((id & 0xFFFF) == 0xFFFF)) {
        return ESP_ERR_FLASH_UNSUPPORTED_CHIP;
    }

    *size = 1 << (id & 0xFF);
    return ESP_OK;
}


esp_err_t spi_flash_chip_generic_erase_chip(esp_flash_t *chip)
{
    esp_err_t err;

    err = chip->chip_drv->set_chip_write_protect(chip, false);
    if (err == ESP_OK) {
        err = chip->chip_drv->wait_idle(chip, chip->chip_drv->timeout->idle_timeout);
    }
    if (err == ESP_OK) {
        chip->host->driver->erase_chip(chip->host);
        //to save time, flush cache here
        if (chip->host->driver->flush_cache) {
            err = chip->host->driver->flush_cache(chip->host, 0, chip->size);
            if (err != ESP_OK) {
                return err;
            }
        }
        err = chip->chip_drv->wait_idle(chip, chip->chip_drv->timeout->chip_erase_timeout);
    }
    return err;
}

esp_err_t spi_flash_chip_generic_erase_sector(esp_flash_t *chip, uint32_t start_address)
{
    esp_err_t err = chip->chip_drv->set_chip_write_protect(chip, false);
    if (err == ESP_OK) {
        err = chip->chip_drv->wait_idle(chip, chip->chip_drv->timeout->idle_timeout);
    }
    if (err == ESP_OK) {
        chip->host->driver->erase_sector(chip->host, start_address);
        //to save time, flush cache here
        if (chip->host->driver->flush_cache) {
            err = chip->host->driver->flush_cache(chip->host, start_address, chip->chip_drv->sector_size);
            if (err != ESP_OK) {
                return err;
            }
        }
        err = chip->chip_drv->wait_idle(chip, chip->chip_drv->timeout->sector_erase_timeout);
    }
    return err;
}

esp_err_t spi_flash_chip_generic_erase_block(esp_flash_t *chip, uint32_t start_address)
{
    esp_err_t err = chip->chip_drv->set_chip_write_protect(chip, false);
    if (err == ESP_OK) {
        err = chip->chip_drv->wait_idle(chip, chip->chip_drv->timeout->idle_timeout);
    }
    if (err == ESP_OK) {
        chip->host->driver->erase_block(chip->host, start_address);
        //to save time, flush cache here
        if (chip->host->driver->flush_cache) {
            err = chip->host->driver->flush_cache(chip->host, start_address, chip->chip_drv->block_erase_size);
            if (err != ESP_OK) {
                return err;
            }
        }
        err = chip->chip_drv->wait_idle(chip, chip->chip_drv->timeout->block_erase_timeout);
    }
    return err;
}

esp_err_t spi_flash_chip_generic_read(esp_flash_t *chip, void *buffer, uint32_t address, uint32_t length)
{
    esp_err_t err = ESP_OK;
    const uint32_t page_size = chip->chip_drv->page_size;
    uint32_t align_address;
    uint8_t temp_buffer[64]; //spiflash hal max length of read no longer than 64byte

    // Configure the host, and return
    err = spi_flash_chip_generic_config_host_io_mode(chip);

    if (err == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGE(TAG, "configure host io mode failed - unsupported");
        return err;
    }

    while (err == ESP_OK && length > 0) {
        memset(temp_buffer, 0xFF, sizeof(temp_buffer));
        uint32_t read_len = chip->host->driver->read_data_slicer(chip->host, address, length, &align_address, page_size);
        uint32_t left_off = address - align_address;
        uint32_t data_len = MIN(align_address + read_len, address + length) - address;
        err = chip->host->driver->read(chip->host, temp_buffer, align_address, read_len);

        memcpy(buffer, temp_buffer + left_off, data_len);

        address += data_len;
        buffer = (void *)((intptr_t)buffer + data_len);
        length = length - data_len;
    }

    return err;
}

esp_err_t spi_flash_chip_generic_page_program(esp_flash_t *chip, const void *buffer, uint32_t address, uint32_t length)
{
    esp_err_t err;

    err = chip->chip_drv->wait_idle(chip, chip->chip_drv->timeout->idle_timeout);

    if (err == ESP_OK) {
        // Perform the actual Page Program command
        chip->host->driver->program_page(chip->host, buffer, address, length);

        err = chip->chip_drv->wait_idle(chip, chip->chip_drv->timeout->page_program_timeout);
    }
    return err;
}

esp_err_t spi_flash_chip_generic_write(esp_flash_t *chip, const void *buffer, uint32_t address, uint32_t length)
{
    esp_err_t err = ESP_OK;
    const uint32_t page_size = chip->chip_drv->page_size;
    uint32_t align_address;
    uint8_t temp_buffer[64]; //spiflash hal max length of write no longer than 64byte

    while (err == ESP_OK && length > 0) {
        memset(temp_buffer, 0xFF, sizeof(temp_buffer));
        uint32_t page_len = chip->host->driver->write_data_slicer(chip->host, address, length, &align_address, page_size);
        uint32_t left_off = address - align_address;
        uint32_t write_len = MIN(align_address + page_len, address + length) - address;
        memcpy(temp_buffer + left_off, buffer, write_len);

        err = chip->chip_drv->set_chip_write_protect(chip, false);
        if (err == ESP_OK && length > 0) {
            err = chip->chip_drv->program_page(chip, temp_buffer, align_address, page_len);

            address += write_len;
            buffer = (void *)((intptr_t)buffer + write_len);
            length -= write_len;
        }
    }
    if (err == ESP_OK && chip->host->driver->flush_cache) {
        err = chip->host->driver->flush_cache(chip->host, address, length);
    }
    return err;
}

esp_err_t spi_flash_chip_generic_write_encrypted(esp_flash_t *chip, const void *buffer, uint32_t address, uint32_t length)
{
    return ESP_ERR_FLASH_UNSUPPORTED_HOST; // TODO
}

esp_err_t spi_flash_chip_generic_set_write_protect(esp_flash_t *chip, bool write_protect)
{
    esp_err_t err = ESP_OK;

    err = chip->chip_drv->wait_idle(chip, chip->chip_drv->timeout->idle_timeout);

    if (err == ESP_OK) {
        chip->host->driver->set_write_protect(chip->host, write_protect);
    }

    bool wp_read;
    err = chip->chip_drv->get_chip_write_protect(chip, &wp_read);
    if (err == ESP_OK && wp_read != write_protect) {
        // WREN flag has not been set!
        err = ESP_ERR_NOT_FOUND;
    }
    return err;
}

esp_err_t spi_flash_chip_generic_get_write_protect(esp_flash_t *chip, bool *out_write_protect)
{
    esp_err_t err = ESP_OK;
    uint8_t status;
    assert(out_write_protect!=NULL);
    err = chip->host->driver->read_status(chip->host, &status);
    if (err != ESP_OK) {
        return err;
    }

    *out_write_protect = ((status & SR_WREN) == 0);
    return err;
}

esp_err_t spi_flash_generic_wait_host_idle(esp_flash_t *chip, uint32_t *timeout_us)
{
    while (chip->host->driver->host_idle(chip->host) && *timeout_us > 0) {
#if HOST_DELAY_INTERVAL_US > 0
        if (*timeout_us > 1) {
            int delay = MIN(HOST_DELAY_INTERVAL_US, *timeout_us);
            chip->os_func->delay_us(chip->os_func_data, delay);
            *timeout_us -= delay;
        } else {
            return ESP_ERR_TIMEOUT;
        }
#endif
    }
    return ESP_OK;
}

esp_err_t spi_flash_chip_generic_wait_idle(esp_flash_t *chip, uint32_t timeout_us)
{
    timeout_us++; // allow at least one pass before timeout, last one has no sleep cycle

    uint8_t status = 0;
    const int interval = CHIP_WAIT_IDLE_INTERVAL_US;
    while (timeout_us > 0) {

        esp_err_t err = spi_flash_generic_wait_host_idle(chip, & timeout_us);
        if (err != ESP_OK) {
            return err;
        }

        err = chip->host->driver->read_status(chip->host, &status);
        if (err != ESP_OK) {
            return err;
        }
        if ((status & SR_WIP) == 0) {
            break; // Write in progress is complete
        }
        if (timeout_us > 0 && interval > 0) {
            int delay = MIN(interval, timeout_us);
            chip->os_func->delay_us(chip->os_func_data, delay);
            timeout_us -= delay;
        }
    }

    return (timeout_us > 0) ?  ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t spi_flash_chip_generic_config_host_io_mode(esp_flash_t *chip)
{
    uint32_t dummy_cyclelen_base;
    uint32_t addr_bitlen;
    uint32_t read_command;

    switch (chip->read_mode) {
    case SPI_FLASH_QIO:
        //for QIO mode, the 4 bit right after the address are used for continuous mode, should be set to 0 to avoid that.
        addr_bitlen = SPI_FLASH_QIO_ADDR_BITLEN;
        dummy_cyclelen_base = rom_flash_chip_dummy->qio_dummy_bitlen;
        read_command = CMD_FASTRD_QIO;
        break;
    case SPI_FLASH_QOUT:
        addr_bitlen = SPI_FLASH_QOUT_ADDR_BITLEN;
        dummy_cyclelen_base = rom_flash_chip_dummy->qout_dummy_bitlen;
        read_command = CMD_FASTRD_QUAD;
        break;
    case SPI_FLASH_DIO:
        //for DIO mode, the 4 bit right after the address are used for continuous mode, should be set to 0 to avoid that.
        addr_bitlen = SPI_FLASH_DIO_ADDR_BITLEN;
        dummy_cyclelen_base = rom_flash_chip_dummy->dio_dummy_bitlen;
        read_command = CMD_FASTRD_DIO;
        break;
    case SPI_FLASH_DOUT:
        addr_bitlen = SPI_FLASH_DOUT_ADDR_BITLEN;
        dummy_cyclelen_base = rom_flash_chip_dummy->dout_dummy_bitlen;
        read_command = CMD_FASTRD_DUAL;
        break;
    case SPI_FLASH_FASTRD:
        addr_bitlen = SPI_FLASH_FASTRD_ADDR_BITLEN;
        dummy_cyclelen_base = rom_flash_chip_dummy->fastrd_dummy_bitlen;
        read_command = CMD_FASTRD;
        break;
    case SPI_FLASH_SLOWRD:
        addr_bitlen = SPI_FLASH_SLOWRD_ADDR_BITLEN;
        dummy_cyclelen_base = rom_flash_chip_dummy->slowrd_dummy_bitlen;
        read_command = CMD_READ;
        break;
    default:
        return ESP_ERR_FLASH_NOT_INITIALISED;
    }

    return chip->host->driver->configure_host_io_mode(chip->host, read_command, addr_bitlen, dummy_cyclelen_base,
                                                chip->read_mode);
}

esp_err_t spi_flash_chip_generic_get_io_mode(esp_flash_t *chip, esp_flash_io_mode_t* out_io_mode)
{
    // On "generic" chips, this involves checking
    // bit 1 (QE) of RDSR2 (35h) result
    // (it works this way on GigaDevice & Fudan Micro chips, probably others...)
    const uint8_t BIT_QE = 1 << 1;
    uint32_t sr;
    esp_err_t ret = spi_flash_common_read_status_8b_rdsr2(chip, &sr);
    if (ret == ESP_OK) {
        *out_io_mode = ((sr & BIT_QE)? SPI_FLASH_QOUT: 0);
    }
    return ret;
}

esp_err_t spi_flash_chip_generic_set_io_mode(esp_flash_t *chip)
{
    // On "generic" chips, this involves checking
    // bit 9 (QE) of RDSR (05h) result
    const uint32_t BIT_QE = 1 << 9;
    return spi_flash_common_set_io_mode(chip,
                                        spi_flash_common_write_status_16b_wrsr,
                                        spi_flash_common_read_status_16b_rdsr_rdsr2,
                                        BIT_QE);
}

static const char chip_name[] = "generic";

const spi_flash_chip_t esp_flash_chip_generic = {
    .name = chip_name,
    .timeout = &spi_flash_chip_generic_timeout,
    .probe = spi_flash_chip_generic_probe,
    .reset = spi_flash_chip_generic_reset,
    .detect_size = spi_flash_chip_generic_detect_size,
    .erase_chip = spi_flash_chip_generic_erase_chip,
    .erase_sector = spi_flash_chip_generic_erase_sector,
    .erase_block = spi_flash_chip_generic_erase_block,
    .sector_size = 4 * 1024,
    .block_erase_size = 64 * 1024,

    // TODO: figure out if generic chip-wide protection bits exist across some manufacturers
    .get_chip_write_protect = spi_flash_chip_generic_get_write_protect,
    .set_chip_write_protect = spi_flash_chip_generic_set_write_protect,

    // Chip write protection regions do not appear to be standardised
    // at all, this is implemented in chip-specific drivers only.
    .num_protectable_regions = 0,
    .protectable_regions = NULL,
    .get_protected_regions = NULL,
    .set_protected_regions = NULL,

    .read = spi_flash_chip_generic_read,
    .write = spi_flash_chip_generic_write,
    .program_page = spi_flash_chip_generic_page_program,
    .page_size = 256,
    .write_encrypted = spi_flash_chip_generic_write_encrypted,

    .wait_idle = spi_flash_chip_generic_wait_idle,
    .set_io_mode = spi_flash_chip_generic_set_io_mode,
    .get_io_mode = spi_flash_chip_generic_get_io_mode,
};

/*******************************************************************************
 * Utility functions
 ******************************************************************************/

static esp_err_t spi_flash_common_read_qe_sr(esp_flash_t *chip, uint8_t qe_rdsr_command, uint8_t qe_sr_bitwidth, uint32_t *sr)
{
    uint32_t sr_buf = 0;
    spi_flash_trans_t t = {
        .command = qe_rdsr_command,
        .miso_data = (uint8_t*) &sr_buf,
        .miso_len = qe_sr_bitwidth / 8,
    };
    esp_err_t ret = chip->host->driver->common_command(chip->host, &t);
    *sr = sr_buf;
    return ret;
}

static esp_err_t spi_flash_common_write_qe_sr(esp_flash_t *chip, uint8_t qe_wrsr_command, uint8_t qe_sr_bitwidth, uint32_t qe)
{
    spi_flash_trans_t t = {
        .command = qe_wrsr_command,
        .mosi_data = ((uint8_t*) &qe),
        .mosi_len = qe_sr_bitwidth / 8,
        .miso_len = 0,
    };
    return chip->host->driver->common_command(chip->host, &t);
}

esp_err_t spi_flash_common_read_status_16b_rdsr_rdsr2(esp_flash_t* chip, uint32_t* out_sr)
{
    uint32_t sr, sr2;
    esp_err_t ret = spi_flash_common_read_qe_sr(chip, CMD_RDSR2, 8, &sr2);
    if (ret == ESP_OK) {
        ret = spi_flash_common_read_qe_sr(chip, CMD_RDSR, 8, &sr);
    }
    if (ret == ESP_OK) {
        *out_sr = (sr & 0xff) | ((sr2 & 0xff) << 8);
    }
    return ret;
}

esp_err_t spi_flash_common_read_status_8b_rdsr2(esp_flash_t* chip, uint32_t* out_sr)
{
    return spi_flash_common_read_qe_sr(chip, CMD_RDSR2, 8, out_sr);
}

esp_err_t spi_flash_common_read_status_8b_rdsr(esp_flash_t* chip, uint32_t* out_sr)
{
    return spi_flash_common_read_qe_sr(chip, CMD_RDSR, 8, out_sr);
}

esp_err_t spi_flash_common_write_status_16b_wrsr(esp_flash_t* chip, uint32_t sr)
{
    return spi_flash_common_write_qe_sr(chip, CMD_WRSR, 16, sr);
}

esp_err_t spi_flash_common_write_status_8b_wrsr(esp_flash_t* chip, uint32_t sr)
{
    return spi_flash_common_write_qe_sr(chip, CMD_WRSR, 8, sr);
}

esp_err_t spi_flash_common_write_status_8b_wrsr2(esp_flash_t* chip, uint32_t sr)
{
    return spi_flash_common_write_qe_sr(chip, CMD_WRSR2, 8, sr);
}

esp_err_t spi_flash_common_set_io_mode(esp_flash_t *chip, esp_flash_wrsr_func_t wrsr_func, esp_flash_rdsr_func_t rdsr_func, uint32_t qe_sr_bit)
{
    esp_err_t ret = ESP_OK;
    const bool is_quad_mode = esp_flash_is_quad_mode(chip);
    bool update_config = false;
    /*
     * By default, we don't clear the QE bit even the flash mode is not QIO or QOUT. Force clearing
     * QE bit by the generic chip driver (command 01H with 2 bytes) may cause the output of some
     * chips (MXIC) no longer valid.
     * Enable this option when testing a new flash chip for clearing of QE.
     */
    const bool force_check = false;

    bool need_check = is_quad_mode || force_check;

    uint32_t sr_update;
    if (need_check) {
        // Ensure quad modes are enabled, using the Quad Enable parameters supplied.
        uint32_t sr;
        ret = (*rdsr_func)(chip, &sr);
        if (ret != ESP_OK) {
            return ret;
        }
        ESP_EARLY_LOGD(TAG, "set_io_mode: status before 0x%x", sr);
        if (is_quad_mode) {
            sr_update = sr | qe_sr_bit;
        } else {
            sr_update = sr & (~qe_sr_bit);
        }
        ESP_EARLY_LOGV(TAG, "set_io_mode: status update 0x%x", sr_update);
        if (sr != sr_update) {
            update_config = true;
        }
    }

    if (update_config) {
        //some chips needs the write protect to be disabled before writing to Status Register
        chip->chip_drv->set_chip_write_protect(chip, false);

        ret = (*wrsr_func)(chip, sr_update);
        if (ret != ESP_OK) {
            return ret;
        }

        ret = chip->chip_drv->wait_idle(chip, chip->chip_drv->timeout->idle_timeout);
        if (ret != ESP_OK) {
            return ret;
        }

        /* Check the new QE bit has stayed set */
        uint32_t sr;
        ret = (*rdsr_func)(chip, &sr);
        if (ret != ESP_OK) {
            return ret;
        }
        ESP_EARLY_LOGD(TAG, "set_io_mode: status after 0x%x", sr);
        if (sr != sr_update) {
            ret = ESP_ERR_FLASH_NO_RESPONSE;
        }

        chip->chip_drv->set_chip_write_protect(chip, true);
    }
    return ret;
}
