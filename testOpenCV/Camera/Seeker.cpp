#include "Seeker.h"
#include "CV/HoughLinesR.h"
#include "namespaces/filters.h"
#include "CV/HoughLinesPR.h"

Seeker::Seeker()
    : current_phase_(Phase::ONE)
    , current_frameset_(nullptr) {
    phase_roi_[0] = def_phase_one_roi_;
}

Seeker::Seeker(capture_roi phase_one_roi)
    : current_phase_(Phase::ONE)
    , current_frameset_(nullptr) {
    phase_roi_[0] = phase_one_roi;
}

bool Seeker::initialize() {

    // generate phase frameset pointers
    for (auto i = 0; i < frameset_.size(); i++)
        frameset_[i] = std::make_unique<Frames>(i);

    if (pcapture == nullptr) {
        // run the basic process for the capture object
        pcapture = std::make_unique<CapturePvApi>();
    } else {
        // double check for weirdness
        if (pcapture->is_open()) {
            pcapture->close();
        }
    }

    // always perform complete re-init.

    auto capture_device_ok = pcapture->initialize();

    if (!capture_device_ok) {
        log_time << "Capture device could not be initialized, aborting.\n";
        return false;
    }

    //initVideoCapture(); // for opencv capture object

    capture_device_ok = pcapture->open();

    if (!capture_device_ok) {
        log_time << "Capture device could not be openened, aborting.\n";
        pcapture->initialized(false);
        return false;
    }

    //capture->print_attr();

    pcapture->pixel_format();

    pcapture->reset_binning();

    pcapture->packet_size(8228);

    // switch frameset
    auto frame_switch = frameset(current_phase_);

    if (frame_switch == -1) {
        log_time << "Frameset configured.\n";
        return false;
    }

    current_frameset_ = frameset_[frame_switch].get();

    auto ok = pcapture->frame_init();
    if (!ok)
        return false;

    ok = pcapture->cap_init();
    if (!ok)
        return false;

    ok = pcapture->aquisition_init();
    if (!ok)
        return false;

    ok = pcapture->region(phase_roi_[frame_switch]);
    if (!ok)
        return false;

    return ok;
}

bool Seeker::shut_down() const {
    pcapture->aquisition_end();
    pcapture->cap_end();
    pcapture->close();
    pcapture->uninitialize();
    return true;
}

void Seeker::switch_phase() {
    switch (current_phase_) {
        case Phase::NONE:
            current_phase_ = Phase::ONE;
            break;
        case Phase::ONE:
            current_phase_ = Phase::TWO;
            break;
        case Phase::TWO:
            current_phase_ = Phase::THREE;
            break;
        case Phase::THREE:
            current_phase_ = Phase::DONE;
            break;
        case Phase::DONE:
            current_phase_ = Phase::NONE;
            break;
        default:
            current_phase_ = Phase::FAIL;
    }

}

void Seeker::phase_one() {

    log_time << "Configuring phase one.\n";

    auto phase = frameset(current_phase_);

    // configure hough for this phase.. the int cast is only used for UI purpose.
    auto hough_vertical = make_shared<HoughLinesR>(1, static_cast<const int>(calc::DEGREES), 40, false);
    hough_vertical->angle_limit(30);

    pfilter->kernel(filters::kernel_line_left_to_right);

    // me not know what long they is
    // TODO : temporary structure, vectors always have a single element in them!
    vector<cv::Rect2d> markings;
    vector<cv::Vec4d> left_borders;
    vector<cv::Vec4d> right_borders;

    // the output types
    cv::Rect2d output(0.0, 0.0, 0.0, 0.0);
    cv::Vec4d left_border_result(0.0, 0.0, 0.0, 0.0);
    cv::Vec4d right_border_result(0.0, 0.0, 0.0, 0.0);

    std::vector<ulong> exposures;
    for (auto i = exposure_levels->exposure_start; i <= exposure_levels->exposure_end; i += exposure_levels->exposure_increment)
        exposures.emplace_back(i);

    std::vector<cv::Mat> targets;
    targets.reserve(1); // lel

    auto running = true;

    auto failures = 0;

    log_time << "Running phase one.\n";

    while (running) {
        try {

            for (const auto e : exposures) {

                pcapture->exposure(e);

                targets.clear();
                pcapture->cap(1, targets);

                auto current_frame = targets.front();

                markings.clear();
                left_borders.clear();
                right_borders.clear();

                pfilter->image(current_frame);
                pfilter->do_filter();

                pcanny->image(pfilter->result());
                pcanny->do_canny();

                auto t = pcanny->result();

                auto tmp = t.clone();
                hough_vertical->original(tmp);
                hough_vertical->image(t);

                if (hough_vertical->hough_vertical() < 0) {
                    log_time << cv::format("No lines detected through hough at exposure level %i.\n", e);
                    continue;
                }

                if (!hough_vertical->is_lines_intersecting(HoughLinesR::Side::Left))
                    continue;

                if (!hough_vertical->is_lines_intersecting(HoughLinesR::Side::Right))
                    continue;

                hough_vertical->compute_borders();

                if (validate::validate_rect(hough_vertical->marking_rect()))
                    markings.emplace_back(hough_vertical->marking_rect());

                if (validate::valid_vec(hough_vertical->left_border()))
                    left_borders.emplace_back(hough_vertical->left_border());

                if (validate::valid_vec(hough_vertical->right_border()))
                    right_borders.emplace_back(hough_vertical->right_border());

                // check for fatal zero
                if (markings.size() + left_borders.size() + right_borders.size() != 0)
                    continue;

                running = false;
                break;

            }

            // TODO : temporary structure, vectors always have a single element in them!
            // set up the avg of the detected markings and borders.
            cvr::avg_vecrect_x_width(markings, output);
            output.y = 0.0;
            output.height = static_cast<double>(phase_roi_[phase].height);

            cvr::avg_vector_vec(left_borders, left_border_result);
            left_border_result[1] = 0.0;
            left_border_result[3] = static_cast<double>(phase_roi_[phase].height);

            cvr::avg_vector_vec(right_borders, right_border_result);
            left_border_result[1] = static_cast<double>(phase_roi_[phase].height);
            left_border_result[3] = 0.0;

            // TODO : adjust the heights to match the current ROI

            for (const auto& lb : left_borders) {
                if (!validate::valid_vec(lb)) {
                    failures++;
                    log_time << __FUNCTION__ << " left_borders validation fail for " << lb << std::endl;
                }
            }

            for (const auto& lb : right_borders) {
                if (!validate::valid_vec(lb)) {
                    failures++;
                    log_time << __FUNCTION__ << " right_borders validation fail for " << lb << std::endl;
                }
            }

        } catch (cv::Exception& e) {
            log_time << "CV Exception\n" << e.what();
            exit(-991);
        } catch (NoLineDetectedException& e) {
            log_time << cv::format("NoLineDetectedException : %s\n", e.what());
            exit(-100);
        }

        if (failures == 0)
            running = false;

    }

    /*   for (auto& fs : frameset_) {
           pcapture->exposure(fs->exp_ms_);
           pcapture->cap(25, fs->frames_);
       }*/

}

void Seeker::phase_two() {

    switch_phase();

    auto phase = frameset(current_phase_);

    // make sure the minimum is at least 10 pixels.
    auto min_line_len = calc::line::compute_houghp_min_line(10.0, pdata->marking_rect);

    // horizontal houghline extension class
    auto hough_horizontal = make_shared<HoughLinesPR>(1, calc::round(calc::DEGREES), 40, calc::round(min_line_len), false);

    hough_horizontal->max_line_gab(12);
    hough_horizontal->marking_rect(pdata->marking_rect);

}

int Seeker::frameset(Phase phase) {
    switch (phase) {
        case Phase::ONE:
            return 0;
        case Phase::TWO:
            return 1;
        case Phase::THREE:
            return 2;
        default:
            return -1;
    }
}