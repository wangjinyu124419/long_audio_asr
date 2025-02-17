#include <iostream>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>

//SDK interface
#include "BDSpeechSDK.hpp"
#include "bds_ASRDefines.hpp"
#include "bds_asr_key_definitions.hpp"
#include "BaiduSpeechAPI.hpp"

#define MAX_OUT_BUF_SIZE	8192

using namespace std;

/**
 * 0. demo不用修改任何代码，可以直接运行。测试成功后请进行以下步骤
 * 1. 请修改asr_set_config_params 函数，配置您的参数。
 * 2. THREAD_NUM 修改同时进行的识别线程，
 * 3. 测试完毕后，请确认修改asr_set_start_params里面app参数
 */

char audio_dir[256] = "./pcm"; // 与THREAD_NUM一起决定测试的文件名
const int THREAD_NUM = 1; // 测试的线程数，最大不能超过10
pthread_t thread_ids[THREAD_NUM]; // 线程信息
pthread_mutex_t thread_mutexes[THREAD_NUM]; // 锁信息，用于同步SDK内部回调线程和用户的调用线程的asr_finish_tags
bool asr_finish_tags[THREAD_NUM] = {0}; //线程是否结束识别
int thread_sequeces[THREAD_NUM] = {0};
char file_names[THREAD_NUM][256]; // 每个线程识别的文件名
unsigned int out_buf_index = 0;
char out_buf[MAX_OUT_BUF_SIZE];

/**
 * 格式化时间
 */
std::string get_gmt_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm current_time;
    localtime_r(&tv.tv_sec, &current_time);

    int year = (1900 + current_time.tm_year);
    int month = (1 + current_time.tm_mon);
    int day = current_time.tm_mday;
    int hour = current_time.tm_hour;
    int minute = current_time.tm_min;
    int sec = current_time.tm_sec;
    int msec = (int) (tv.tv_usec / 1000);

    char time_ch_buf[128] = {0};
    snprintf(time_ch_buf, sizeof(time_ch_buf) / sizeof(char), "%d-%02d-%02d %02d:%02d:%02d.%03d", year, month, day,
             hour, minute, sec, msec);
    return std::string(time_ch_buf);
}

/**
 * 请根据文档说明设置参数
 */
void asr_set_config_params(bds::BDSSDKMessage &cfg_params) {
    //const bds::TBDVoiceRecognitionDebugLogLevel sdk_log_level = bds::EVRDebugLogLevelTrace;
    const bds::TBDVoiceRecognitionDebugLogLevel sdk_log_level = bds::EVRDebugLogLevelOff; // 关闭详细日志

    // app_id app_key app_secret 请测试成功后替换为您在网页上申请的appId appKey和appSecret
    const std::string app_id = "10555002";
    const std::string chunk_key = "jhRA15uv8Lvd4r9qbtmOODMv";
    const std::string secret_key = "f0a12f8261e1121861a1cd3f4ed02f68";

    const std::string product_id = "15362";
    // const std::string product_id = "1536";// 普通话搜索模型：1536，普通话搜索模型+语义理解 15361, 普通话输入法模型（有逗号） 1537

    cfg_params.name = bds::ASR_CMD_CONFIG;

    cfg_params.set_parameter(bds::ASR_PARAM_KEY_APP_ID, app_id);
    cfg_params.set_parameter(bds::ASR_PARAM_KEY_CHUNK_KEY, chunk_key);

    cfg_params.set_parameter(bds::ASR_PARAM_KEY_PRODUCT_ID, product_id);
    cfg_params.set_parameter(bds::COMMON_PARAM_KEY_DEBUG_LOG_LEVEL, sdk_log_level);

    // float vad_pause_frames = 30;  //设置vad语句静音切分门限（帧）, 30帧 = 300ms
    // cfg_params.set_parameter(bds::ASR_PARAM_KEY_MAX_SPEECH_PAUSE, vad_pause_frames);


    // cfg_params.set_parameter(bds::ASR_PARAM_KEY_SAVE_AUDIO_ENABLE, 1);    //是否存识别的音频
    // cfg_params.set_parameter(bds::ASR_PARAM_KEY_SAVE_AUDIO_PATH, "sdk_save_audio.d");  //存音频的路径

    cfg_params.set_parameter(bds::ASR_PARAM_KEY_ENABLE_LONG_SPEECH, 1); // 强制固定值
    cfg_params.set_parameter(bds::ASR_PARAM_KEY_CHUNK_ENABLE, 1); // 强制固定值
    const std::string mfe_dnn_file_path = "../../resources/asr_resource/bds_easr_mfe_dnn.dat"; //  bds_easr_mfe_dnn.dat文件路径
    const std::string mfe_cmvn_file_path = "../../resources/asr_resource/bds_easr_mfe_cmvn.dat"; //  bds_easr_mfe_cmvn.dat文件路径
    cfg_params.set_parameter(bds::ASR_PARAM_KEY_MFE_DNN_DAT_FILE, mfe_dnn_file_path); // 强制固定值
    cfg_params.set_parameter(bds::ASR_PARAM_KEY_MFE_CMVN_DAT_FILE, mfe_cmvn_file_path); // 强制固定值
    cfg_params.set_parameter(bds::ASR_PARAM_KEY_COMPRESSION_TYPE, bds::EVR_AUDIO_COMPRESSION_PCM);
    // cfg_params.set_parameter(bds::ASR_PARAM_KEY_COMPRESSION_TYPE, bds::EVR_AUDIO_COMPRESSION_BV32); // 有损压缩, 可能遇见音频压缩问题

}

// 设置启动参数
void asr_set_start_params(bds::BDSSDKMessage &start_params) {
    const std::string app = "YourOwnName";

    start_params.name = bds::ASR_CMD_START;
    start_params.set_parameter(bds::ASR_PARAM_KEY_APP, app);
    start_params.set_parameter(bds::ASR_PARAM_KEY_PLATFORM, "linux"); //固定值
    start_params.set_parameter(bds::ASR_PARAM_KEY_SDK_VERSION, "LINUX TEST"); //固定值
}

/**
 * @param file_path IN 文件路径
 * @param push_cmd IN 固定值 bds::ASR_CMD_PUSH_AUDIO
 * @param sdk BDSpeechSDK
 * @param thread_seq demo里的线程编号 用于获取如asr_finish_tags[thread_seq]线程相关的信息
 */
int asr_online_pushaudio(const char *file_path, const std::string &push_cmd, bds::BDSpeechSDK *sdk, int thread_seq) {
    const int audio_buf_len = 320; // 建议320字节一个包
    const double per_send_seconds = 0.01; //320字节 ，16000采样率 sleep 10ms， sleep时间与包大小成正比
    char audio_buf[audio_buf_len];

    FILE *err_output_file = stderr;

    FILE *fp = fopen(file_path, "rb");

    if (!fp) {
        fprintf(err_output_file, "open audio data failed\n");
        return 1;
    }

    std::string err_msg;
    bds::BDSSDKMessage push_params;
    push_params.name = push_cmd;

    bool asr_finished = false;

    while (!feof(fp) && !asr_finished) {
        size_t read_cnt = fread(audio_buf, 1, audio_buf_len, fp);
        if (read_cnt > 0) {
            push_params.set_parameter(bds::DATA_CHUNK, audio_buf, (int) read_cnt);
            //printf("[%s]push_audio data, size %ld\n", get_gmt_time().c_str(), read_cnt);
            if (!sdk->post(push_params, err_msg)) {
                fprintf(err_output_file, "push audio data failed for %s\n", err_msg.c_str());
            }
            //考虑到解码器的识别速度，此处的sleep必不可少，否则影响识别
            usleep(static_cast<long>(per_send_seconds * 1000 * 1000));
        }

        asr_finished = asr_finish_tags[thread_seq];
    }

    //告诉sdk，后续不会再post音频数据 ， 注意这个调用之后需要紧接着调用asr_online_stop
    push_params.set_parameter(bds::DATA_CHUNK, audio_buf, 0);
    printf("[%s]push_audio finish\n", get_gmt_time().c_str());
    if (!sdk->post(push_params, err_msg)) {
        fprintf(err_output_file, "push audio data failed for %s\n", err_msg.c_str());
    }

    fclose(fp);
    return 0;
}

/**
 * SDK 识别过程中的回调，注意回调产生在SDK内部的线程中，并非调用线程。
 * @param message IN SDK的回调信息
 *
 */
void asr_output_callback(bds::BDSSDKMessage &message, void *user_arg) {
	int thread_seq = 0;
    FILE *err_output_file = stderr;
    char* out_buf = (char*)user_arg;


    if (message.name != bds::asr_callback_name) {
        fprintf(err_output_file, "shouldn't call\n");
        return;
    }

    int status = 0;

    if (!message.get_parameter(bds::CALLBACK_ASR_STATUS, status)) {
        fprintf(err_output_file, "get status failed\n");
        return;
    }

    FILE *result_output_file = stdout;
    const char *time = get_gmt_time().c_str();
    switch (status) {
        case bds::EVoiceRecognitionClientWorkStatusStartWorkIng: {
            fprintf(result_output_file, "[%s]识别工作开始\n", time);
            fflush(result_output_file);
            break;
        }

        case bds::EVoiceRecognitionClientWorkStatusStart: { // 一句话开始
            fprintf(result_output_file, "[%s]检测到开始说话\n", time);
            fflush(result_output_file);
            break;
        }

        case bds::EVoiceRecognitionClientWorkStatusEnd: { // 一句话结束
            fprintf(result_output_file, "[%s]检测到说话结束\n", time);
            fflush(result_output_file);
            break;
        }

        case bds::EVoiceRecognitionClientWorkStatusFlushData: { // 连续上屏,中间结果
            std::string json_result;
            message.get_parameter(bds::CALLBACK_ASR_RESULT, json_result);
            fprintf(result_output_file, "[%s]patial result: %s\n", time, json_result.c_str());
            fflush(result_output_file);
            break;
        }

        case bds::EVoiceRecognitionClientWorkStatusFinish: {    //一句话的最终结果
            std::string json_result;
            message.get_parameter(bds::CALLBACK_ASR_RESULT, json_result);
            int len = json_result.length();
            if(out_buf_index + len > MAX_OUT_BUF_SIZE) {
            	fprintf(result_output_file, "ERROR:out_buf_index is more than %d", MAX_OUT_BUF_SIZE);
            }else {
				memcpy(&out_buf[out_buf_index],json_result.c_str(),len);
				out_buf_index += len;
            }
            fprintf(result_output_file, "[%s]final result: %s\n", time, json_result.c_str());
            fflush(result_output_file);
            break;
        }

        case bds::EVoiceRecognitionClientWorkStatusChunkNlu: { //语义解析
            const char *buf;
            int len = 0;
            message.get_parameter(bds::DATA_CHUNK, buf, len);
            fprintf(result_output_file, "[%s]nlu result:", time);
            for (int i = 0; i < len; ++i) {
                fprintf(result_output_file, "%c", buf[i]);
            }
            fprintf(result_output_file, "\n");
            fflush(result_output_file);
            break;
        }

        case bds::EVoiceRecognitionClientWorkStatusLongSpeechEnd: {
            // 长语音结束状态 该实例处于空闲状态
        	asr_finish_tags[thread_seq] = true;
            fprintf(result_output_file, "识别完成\n");
            fflush(result_output_file);
            break;
        }

        case bds::EVoiceRecognitionClientWorkStatusError: {
            // 产生错误 该实例处于空闲状态
            int err_code = 0;
            int err_domain = 0;
            std::string err_desc;
            message.get_parameter(bds::CALLBACK_ERROR_CODE, err_code);
            message.get_parameter(bds::CALLBACK_ERROR_DOMAIN, err_domain);
            message.get_parameter(bds::CALLBACK_ERROR_DESC, err_desc);

            std::string sn;
            message.get_parameter(bds::CALLBACK_ERROR_SERIAL_NUM, sn);

            asr_finish_tags[thread_seq] = true;
            fprintf(result_output_file,
                    "[%s]识别出错, err_code: %d, err_domain: %d,\
                 err_desc: %s, sn: %s\n",
                    get_gmt_time().c_str(), err_code, err_domain, err_desc.c_str(), sn.c_str());
            fflush(result_output_file);
            break;
        }

        case bds::EVoiceRecognitionClientWorkStatusCancel: {
            // 用户取消 该实例处于空闲状态
        	asr_finish_tags[thread_seq] = true;
            fprintf(result_output_file, "[%s]用户取消\n", get_gmt_time().c_str());
            fflush(result_output_file);
            break;
        }
        case bds::EVoiceRecognitionClientWorkStatusNewRecordData:
        case bds::EVoiceRecognitionClientWorkStatusMeterLevel:
            break;
        default: {
            fprintf(result_output_file, "其它状态%d\n", status);
            break;
        }
    }
}

/**
 * 释放SDK
 */
void asr_online_release(bds::BDSpeechSDK *sdk) {
    bds::BDSpeechSDK::release_instance(sdk);
}

/**
 * 发送停止命令
 */
int asr_online_stop(bds::BDSpeechSDK *sdk) {
    FILE *err_output_file = stderr;
    std::string err_msg;
    bds::BDSSDKMessage stop_params;
    stop_params.name = bds::ASR_CMD_STOP;

    if (!sdk->post(stop_params, err_msg)) {
        fprintf(err_output_file, "stop sdk failed for %s\n", err_msg.c_str());
        return 1;
    }

    return 0;
}

char* asr_request_parse_file(const char *in_file_name) {
	out_buf_index = 0;
	int thread_seq = 0;
    FILE *err_output_file = stderr;
    asr_finish_tags[thread_seq] = false;
    fprintf(stdout, "Will recognize file: %s\n", in_file_name);

    /*  0 设置日志文件路径，如不设置默认输出到stderr     */
    bds::BDSpeechSDK::open_log_file("asr.log", 25); // 与BDSpeechSDK::close_log_file();配对使用。
    // 25 表示 日志文件大小约25*512k， 超过后SDK新建一个日志文件，旧日志文件覆盖到"asr.log.bak"

    /*  1 获取sdk实例   */
    std::string err_msg;
    bds::BDSpeechSDK *sdk = bds::BDSpeechSDK::get_instance(bds::SDK_TYPE_ASR, err_msg);

    if (!sdk) {
        fprintf(err_output_file, "thread %d, get sdk failed for %s\n", thread_seq, err_msg.c_str());
        return 0;
    }
    fprintf(stdout, "sdk version :%s\n", sdk->get_sdk_version().c_str());
    /*  2 设置输出回调  */
    sdk->set_event_listener(&asr_output_callback, (void *) out_buf);

    /*  3 设置并发送sdk配置参数 */
    bds::BDSSDKMessage cfg_params;
    asr_set_config_params(cfg_params);

    if (!sdk->post(cfg_params, err_msg)) {
        fprintf(err_output_file, "thread %d, init sdk failed for %s\n", thread_seq, err_msg.c_str());
        bds::BDSpeechSDK::release_instance(sdk);
        return 0;
    }

    /*  4 设置并发送sdk启动参数 */
    bds::BDSSDKMessage start_params;
    asr_set_start_params(start_params);

    if (!sdk->post(start_params, err_msg)) {
        fprintf(err_output_file, "thread %d, start sdk failed for %s\n", thread_seq, err_msg.c_str());
        bds::BDSpeechSDK::release_instance(sdk);
        return 0;
    }

    /*  5 传输音频数据  */
    int ret = asr_online_pushaudio(in_file_name, bds::ASR_CMD_PUSH_AUDIO, sdk, thread_seq);

    if (ret) {
        fprintf(err_output_file, "thread %d, push audio failed\n", thread_seq);
        asr_online_stop(sdk);
        bds::BDSpeechSDK::release_instance(sdk);
        return 0;
    }

    /*  6 发送停止传输音频数据标记  */
    asr_online_stop(sdk);

    /*  7 等待识别结束 */
    bool asr_finished = false;

    while (!asr_finished) {
        usleep(10000);
        asr_finished = asr_finish_tags[thread_seq];
    }

    /* 8 关闭日志 ，如果之前调用过 open_log_file  */
    bds::BDSpeechSDK::close_log_file();

    /*  8 释放sdk    */
    asr_online_release(sdk);

    fprintf(stdout, "[%s]thread[%d] finish\n", get_gmt_time().c_str(), thread_seq);
    fflush(stdout);

    bds::BDSpeechSDK::do_cleanup();

    out_buf[out_buf_index] = '\0';
	return out_buf;
}

int main(int argc, char **argv) {
	int len;
    FILE *err_output_file = stderr;

    for (int i = 0; i < THREAD_NUM; ++i) {
        snprintf(file_names[i], 256, "%s/16k-%d.pcm", audio_dir, i);
        char* result = asr_request_parse_file(file_names[i]);
        len = strlen(result);
        fprintf(err_output_file, "end of parse.......\n");
        fprintf(err_output_file, "len=[%d]\n", len);
        fflush(err_output_file);
        for (int i = 0; i < len; ++i) {
            fprintf(err_output_file, "%c", result[i]);
        }
        fprintf(err_output_file, "\n");
        fflush(err_output_file);
    }

    //所有任务结束，清理线程池
    bds::BDSpeechSDK::do_cleanup();
    return 0;
}


