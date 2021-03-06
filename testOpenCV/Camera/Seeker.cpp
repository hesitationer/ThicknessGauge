#include "Seeker.h"
#include "CV/HoughLinesR.h"
#include "namespaces/filters.h"
#include "CV/HoughLinesPR.h"
#include "namespaces/draw.h"

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

    auto now = tg::get_now_ns();

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
        log_err << "Capture device could not be initialized, aborting.\n";
        return false;
    }

    //initVideoCapture(); // for opencv capture object

    capture_device_ok = pcapture->open();

    if (!capture_device_ok) {
        log_err << "Capture device could not be openened, aborting.\n";
        pcapture->initialized(false);
        return false;
    }

    //capture->print_attr();

    pcapture->pixel_format();

    pcapture->reset_binning();

    pcapture->packet_size(9014);

    auto def_roi = cv::Rect_<unsigned long>(0, 1006, 2448, 256);

    pcapture->region(def_roi);

    pcapture->frame_init();

    pcapture->cap_init();

    pcapture->aquisition_init();

    log_time << cv::format("Seeker initialize complete, took %i ns.\n", tg::diff_now_ns(now));

    return true;

}

double Seeker::phase_finalize() {

    // alternate version for phase two, contains computation for both sides.
    // uses closest parallel match to counter misbehaviour of laser.

    // note.. the angle detection might not be fine grained enough to detect < 0.3 degrees,
    // this results in this function being completly useless.

    /* (left side illustration, very rough)
      
                    (height)
                    <->     
                   /  /
                  / -/-----(marking)----------------
                 /  /
    -(ground)---/- /
               /  /
     */

    /*
     *  Outline of the process
     *  ----------------------
     *  
     *  - The ground line is determined, and all the points are fitted to a line (a).
     *  - The corresponding half of the line which represents the laser location is also fitted to a line (b).
     *  - Line a and b are compared through log2 of last checked length, this will continue until either:
     *      1) Line angles match
     *      2) Line "distance" is five or less pixels in X, in which case the result is to unreliable
     *  - If 1) is fulfilled, the height is measured from the different between the two lines. *
     *  
     *    
     *     if the angles match, the height of the marking will be set to the distance from a to b.
     *  - 
     *
     *  Line stuff references
     *  ---------------------
     *  - http://mathworld.wolfram.com/Line-LineAngle.html
     *
     * Issues:
     *      
     *  *) The angle measurement might not be good enough to get this.. so a slight margin of error set to .5 degrees
     *      is accepted (see calc.h is_angle_good() function).
     *      This also catches any rounding errors which occoured internaly in one of the many computations in the process.
     *      The build-in angle measurement in opencv promises a error margin of not more than 0.3 degrees.
     *
     */

    // line fitting outputs
    cv::Vec4f line_ground;
    cv::Vec4f line_marking;

    // the point snips
    std::vector<cv::Point2d> points_ground;
    std::vector<cv::Point2d> points_marking;
    points_ground.reserve(DEF_NEAR_EXTRACT);
    points_marking.reserve(DEF_NEAR_EXTRACT);

    log_time << __FUNCTION__ << " begin..\n";

    // extract the values which are near the boundries to optain a very small sample.
    cvr::extract_near<false, false>(pdata->center_points, points_marking, DEF_NEAR_EXTRACT);
    cvr::extract_near<true, false>(pdata->left_points, points_ground, DEF_NEAR_EXTRACT);

    log_time << __FUNCTION__ << cv::format(" points sizes extracted [ground:%i] [marking:%i]\n", points_ground.size(), points_marking.size());

    // set up the configuration for the line fitting
    LineConfig line_config;
    line_config.dist_type(cv::DIST_HUBER);

    log_time << __FUNCTION__ << " fitting lines..\n";

    // fit'em and git'em
    cvr::fit_line(points_ground, line_ground, line_config);
    cvr::fit_line(points_marking, line_marking, line_config);

    log_time << __FUNCTION__ << " angle of line_ground  : " << calc::rad_to_deg(calc::angle(line_ground)) << '\n';
    log_time << __FUNCTION__ << " angle of line_marking : " << calc::rad_to_deg(calc::angle(line_marking)) << '\n';

    log_time << __FUNCTION__ << " computing difference: ";

    auto diff = 0.0;

    // doesn't work
    //for (auto i = 0; i < DEF_NEAR_EXTRACT; i++) {
    //    auto& g = points_ground[i];
    //    auto& m = points_marking[i];
    //    diff += g.y;
    //    diff -= m.y;
    //}
    //diff /= DEF_NEAR_EXTRACT;

    // avg stuff
    auto avg_ground = calc::avg_y(line_ground);
    auto avg_marking = calc::avg_y(line_marking);

    diff = abs(avg_marking - avg_ground);

    std::cout << diff << '\n';

    return diff;
}

bool Seeker::shut_down() const {
    pcapture->aquisition_end();
    pcapture->cap_end();
    pcapture->close();
    pcapture->uninitialize();
    return true;
}

void Seeker::process_mat_for_line(cv::Mat& org, std::shared_ptr<HoughLinesPR>& hough, MorphR* morph) const {
    pfilter->image(org);
    pfilter->do_filter();

    pcanny->image(pfilter->result());
    pcanny->do_canny();

    morph->image(pcanny->result());
    morph->morph();

    hough->image(morph->result());
    hough->hough_horizontal();
}

void Seeker::switch_phase() {
    switch (current_phase_) {
        case Phase::NONE:
            current_phase_ = Phase::ONE;
            break;
        case Phase::ONE:
            current_phase_ = Phase::TWO_LEFT;
            break;
        case Phase::TWO_LEFT:
            current_phase_ = Phase::TWO_RIGHT;
            break;
        case Phase::TWO_RIGHT:
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

bool Seeker::phase_one() {

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

    cv::Mat single_target;
    std::vector<cv::Mat> targets;
    targets.reserve(3); // lel

    auto running = true;

    auto failures = 0;

    // clear any buffer
    log_time << __FUNCTION__ << " clearing the buffer.\n";
    pcapture->region(buffer_clear_roi);
    pcapture->cap(3, targets);
    targets.clear();

    // set capture region at camera level
    pcapture->region(pcapture->default_roi);

    // populate exposures to check
    std::vector<ulong> exposures;
    for (auto i = exposure_levels->exposure_start; i <= exposure_levels->exposure_end; i += exposure_levels->exposure_increment) {
        exposures.emplace_back(i);
    }

    // default amount of frames to capture to make sure there isnt anything in the camera buffer
    auto const frames_to_capture = 3;

    log_time << "Running phase one.\n";

    auto now = tg::get_now_ms();

    // keep going until stopped manually
    while (running) {
        try {

            // iterate through all exposure values
            for (const auto e : exposures) {

                // clear the data structures
                markings.clear();
                left_borders.clear();
                right_borders.clear();

                // set exposure beforehand
                phase_one_exposure = e;

                // if there was an error while switching exposure
                // wait 150ms before skipping
                if (!pcapture->exposure(e)) {
                    log_err << cv::format("%s .. exposure change failed, skipping exposure value %i\n", __FUNCTION__, e);
                    tg::sleep(150);
                    continue;
                }

                // clear targets and capture the frames
                targets.clear();
                pcapture->cap(frames_to_capture, targets);

                auto current_frame = targets.back();
                //cv::imwrite("exposure" + std::to_string(e) + "_1.png", current_frame);

                log_time << __FUNCTION__ << " filter processing..\n";

                // filter the image
                pfilter->image(current_frame);
                pfilter->do_filter();

                //cv::imwrite("exposure" + std::to_string(e) + "_2.png", pfilter->result());

                log_time << __FUNCTION__ << " canny processing..\n";

                // perform edge detection
                pcanny->image(pfilter->result());
                pcanny->do_canny();

                auto t = pcanny->result();
                //cv::imwrite("exposure" + std::to_string(e) + "_3.png", t);

                // process edge image with houghlines
                auto tmp = t.clone();
                hough_vertical->original(tmp);
                hough_vertical->image(t);

                log_time << __FUNCTION__ << " houghline processing..\n";

                auto hough_result = hough_vertical->hough_vertical();
                auto all = hough_vertical->all_lines();

                //hough_vertical->draw_lines(all, cv::Scalar(255, 255, 255));
                //cv::imwrite("exposure" + std::to_string(e) + "_4.png", hough_vertical->output());

                switch (hough_result) {
                    case 0:
                        // everything ok
                        break;
                    case -1:
                        log_err << __FUNCTION__ << " No lines detect.\n";
                        continue;
                    case -2:
                        log_err << __FUNCTION__ << " No valid lines detected.\n";
                        continue;
                    default:
                        // nada
                        break;
                }

                // perform intersection check for left side lines
                if (!hough_vertical->is_lines_intersecting(HoughLinesR::Side::Left)) {
                    log_err << cv::format("Phase one intersection check for left side failed (exposure = %i).\n", phase_one_exposure);
                    continue;
                }

                // perform intersection check for right side lines
                if (!hough_vertical->is_lines_intersecting(HoughLinesR::Side::Right)) {
                    log_err << cv::format("Phase one intersection check for right side failed (exposure = %i).\n", phase_one_exposure);
                    continue;
                }

                // compute the border values from the lines
                hough_vertical->compute_borders();

                // validate the data for abnormalities

                if (validate::validate_rect(hough_vertical->marking_rect())) {
                    markings.emplace_back(hough_vertical->marking_rect());
                }

                if (validate::valid_vec(hough_vertical->left_border())) {
                    left_borders.emplace_back(hough_vertical->left_border());
                }

                if (validate::valid_vec(hough_vertical->right_border())) {
                    right_borders.emplace_back(hough_vertical->right_border());
                }

                // we made it through, allow loop to exit
                running = false;
                break;

            }

            log_time << cv::format("Scan complete.. took %i ms.\n", tg::diff_now_ms(now));

            // seems like the process went ok, do some simply avg calculations of the results.

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

            log_time << __FUNCTION__ " avg calc..\n";

            // TODO : adjust the heights to match the current ROI

            for (const auto& lb : left_borders) {
                if (!validate::valid_vec(lb)) {
                    failures++;
                    log_time << __FUNCTION__ << " left_borders validation fail for " << lb << std::endl;
                }
            }

            log_time << __FUNCTION__ " avg calc..\n";

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

        if (failures == 0) {
            running = false;
        }

    }

    log_time << __FUNCTION__ "  phase one finalize failures : " << failures << '\n';

    try {
        // save the result
        pdata->marking_rect = hough_vertical->marking_rect();

    } catch (std::exception& e) {
        log_err << __FUNCTION__ " exception.. " << e.what();
    }

    log_time << __FUNCTION__ << " marking rect found : " << hough_vertical->marking_rect() << '\n';

    // set phase two roi right away.

    auto quarter = phase_roi_[0].height / 4;
    phase_roi_[1].x = static_cast<unsigned long>(ceil(hough_vertical->marking_rect().x) / 2);
    phase_roi_[1].y = phase_roi_[0].y + 3 * quarter;
    phase_roi_[1].width = phase_roi_[1].x;
    phase_roi_[1].height = quarter;

    log_time << "phase two left region configured : " << phase_roi_[1] << '\n';

    return true;

}

bool Seeker::phase_two_left() {

    log_time << "Phase two configuration started..\n";

    switch_phase();

    std::vector<cv::Mat> left_frames;

    // clear any buffer
    pcapture->region(buffer_clear_roi);
    pcapture->cap(3, left_frames);
    left_frames.clear();

    // prep for next phase
    pcapture->region(phase_roi_[1]);

    auto phase = frameset(current_phase_);

    // make sure the minimum is at least 10 pixels.
    auto min_line_len = calc::line::compute_houghp_min_line(10.0, pdata->marking_rect);

    // horizontal houghline extension class
    auto hough_horizontal = make_shared<HoughLinesPR>(1, calc::round(calc::DEGREES), 40, calc::round(min_line_len), false);
    hough_horizontal->max_line_gab(12);
    hough_horizontal->marking_rect(cv::Rect2d(phase_roi_[1].x, phase_roi_[1].y, phase_roi_[1].width, phase_roi_[1].height));

    //auto left_size = cv::Size(left_baseline.width, left_baseline.height);
    auto left_cutoff = phase_roi_[1].width / 2.0;

    // prepare target element structure
    std::vector<cv::Point2f> elements;
    elements.reserve(512);

    // update base exposure for this phase if it hasnt been configured earlier.
    if (phase_two_base_exposure_ == 0) {
        phase_two_base_exposure_ = phase_one_exposure * 4;
    }

    // configure exposure values for phase two
    std::vector<unsigned long> p2_exposures;
    p2_exposures.reserve(50);
    for (ulong i = 0; i < 50; i++) {
        p2_exposures.push_back(phase_two_base_exposure_ + exposure_levels->exposure_increment * i);
    }

    auto running = true;
    auto found = false;

    cv::Mat org;

    log_time << "Phase two left begun..\n";

    // ************  LEFT SIDE ONLY **************

    const std::string win = "what";

    // attempt to find a good exposure for this phase
    while (running) {

        left_frames.clear();

        found = false;

        for (const auto exp : p2_exposures) {

            // adjust exposure and capture frames
            pcapture->exposure(exp);
            pcapture->cap(2, left_frames);

            // configure structures
            org = left_frames.back().clone();
            auto h = org.clone();
            hough_horizontal->original(h);

            // process matrix for line detection
            process_mat_for_line(org, hough_horizontal, pmorph.get());

            // get results
            const auto& lines = hough_horizontal->all_lines(); // inner most side

            //log_time << __FUNCTION__ << " lines.size() : " << lines.size() << '\n';

            // not enough lines, we are greedy
            if (lines.size() < 2) {
                continue;
            }

            // copy found lines to target structure
            for (auto& line : lines) {
                //if (line.entry_[0] > left_cutoff)                
                stl::copy_vector(line.elements_, elements);
            }

            // check if there is enough to work with
            if (elements.size() > 3) { // && elements.front().y != elements.back().y) {
                phase_two_base_exposure_ = exp;
                running = false;
                found = true;
                break;
            }

            // whoops
            if (exp > 100'000) {
                log_err << "left side failed to produce valid lines.";
                return false;
            }

        }

        if (found) {
            log_time << "Phase two exposure detected.. " << phase_two_base_exposure_ << '\n';
            break;
        }

        log_err << __FUNCTION__ << " unable to detect correct exposure :(\n";
        return false;

    }

    // adjust capture ROI based on found lines.

    auto line_area = cv::minAreaRect(elements);
    auto line_area_rect = line_area.boundingRect();

    log_time << __FUNCTION__ " left boundry detected : " << line_area_rect << '\n';

    auto old_roi = pcapture->region();

    capture_roi new_roi;

    // adjust the new roi according to findings
    new_roi.x = old_roi.x + static_cast<unsigned long>(floor(line_area_rect.x));
    new_roi.y = old_roi.y + static_cast<unsigned long>(floor(line_area_rect.y));
    new_roi.width = static_cast<unsigned long>(ceil(line_area_rect.width));
    new_roi.height = static_cast<unsigned long>(ceil(line_area_rect.height));

    log_time << __FUNCTION__ << " new_roi changed to " << new_roi << '\n';

    // the offset calculation begins with setting the base roi y value
    auto offset_y = static_cast<double>(new_roi.y);
    //auto offset_y = static_cast<double>(phase_roi_[1].y);

    log_time << __FUNCTION__ " init offset_y + new_roi.y : " << offset_y << '\n';

    phase_roi_[1] = new_roi;

    // empty the buffer
    log_time << __FUNCTION__ << " clearing buffer..\n";
    pcapture->region(capture_roi(1, 1, 1, 1));
    pcapture->cap(3, left_frames);
    left_frames.clear();

    // only update region, exposure should be at desired level at this point
    pcapture->region(new_roi);

    // multiply the exposure because it's most likely pretty gloomy there
    pcapture->exposure_mul(DEF_PHASE_TWO_MULTIPLIER);

    running = true;

    auto const frame_count = 25;

    auto left_y = 0.0;

    //cv::namedWindow("morph");

    // capture left frames for real and process the result
    while (running) {

        left_y = 0.0;
        left_frames.clear();
        elements.clear();
        hough_horizontal->clear();

        pcapture->cap(frame_count, left_frames);

        if (left_frames.empty()) {
            log_err << __FUNCTION__ << " fatal error, no frames were captured!\n";
            continue;
        }

        // iterate through the captured frames, don't skip any as the buffer should be alright.
        for (const auto& left : left_frames) {

            org = left.clone();
            auto h = left.clone();
            hough_horizontal->original(h);

            process_mat_for_line(org, hough_horizontal, pmorph.get());

            // grab everything, since we already have defined the roi earlier
            const auto& lines = hough_horizontal->all_lines();

            for (const auto& line : lines) {
                stl::copy_vector(line.elements_, elements);
            }

        }

        if (elements.empty()) {
            log_err << __FUNCTION__ " fatal error, elements are empty!\n";
            return false;
        }

        auto boundry_area = cv::minAreaRect(elements);
        auto boundry_area_rect = boundry_area.boundingRect2f();

        // adjust to reduce crap
        //left_boundry_rect.width -= 40;

        cvr::rect_force_align_boundries<float, 0>(boundry_area_rect, static_cast<float>(left_frames.front().cols), static_cast<float>(left_frames.front().rows));

        if (!validate::validate_rect(boundry_area_rect)) {
            log_err << __FUNCTION__ << " invalid rect in phase two, restarting phase two.\n";
            continue;
        }

        auto t = org(boundry_area_rect);

        try {
            left_y += calc::real_intensity_line(t, pdata->left_points, t.rows, 0);
        } catch (cv::Exception& e) {
            log_err << __FUNCTION__ << " " << e.what() << '\n';
            continue;
        }

        running = false;

    }

    offset_y += left_y;

    log_time << __FUNCTION__ " offset_y + left_y : " << offset_y << '\n';

    pdata->base_lines[1] = offset_y;

    // align points the match the real location in the image.
    for (auto& p : pdata->left_points) {
        p.y += offset_y;
        //std::cout << p << " - ";
    }

    std::cout << '\n';

    const std::string ko = "_phase2_left.png";
    // snap the freaking image
    cv::imwrite(ko, left_frames.front());

    log_time << "left baseline: " << pdata->base_lines[1] << '\n';

    // return exposure to "normal"
    pcapture->exposure_div(DEF_PHASE_TWO_MULTIPLIER);

    // update the phase roi for left side
    //phase_roi<int, 1>(line_area_rect);

    return true;

}

bool Seeker::phase_two_right() {

    log_time << "Phase two configuration started..\n";

    switch_phase();

    std::vector<cv::Mat> right_frames;

    // clear any buffer
    pcapture->region(buffer_clear_roi);
    pcapture->cap(3, right_frames);
    right_frames.clear();

    // prep for next phase
    // TODO : Adjust for right side!
    pcapture->region(phase_roi_[1]);

    auto phase = frameset(current_phase_);

    // make sure the minimum is at least 10 pixels.
    auto min_line_len = calc::line::compute_houghp_min_line(10.0, pdata->marking_rect);

    // horizontal houghline extension class
    auto hough_horizontal = make_shared<HoughLinesPR>(1, calc::round(calc::DEGREES), 40, calc::round(min_line_len), false);
    hough_horizontal->max_line_gab(12);
    hough_horizontal->marking_rect(cv::Rect2d(phase_roi_[1].x, phase_roi_[1].y, phase_roi_[1].width, phase_roi_[1].height));

    //auto right_size = cv::Size(left_baseline.width, left_baseline.height);
    auto right_cutoff = phase_roi_[1].width / 2.0;

    // prepare target element structure
    std::vector<cv::Point2f> elements;
    elements.reserve(512);

    // update base exposure for this phase if it hasnt been configured earlier.
    if (phase_two_base_exposure_ == 0) {
        phase_two_base_exposure_ = phase_one_exposure * 4;
    }

    // configure exposure values for phase two
    std::vector<unsigned long> p2_exposures;
    p2_exposures.reserve(50);
    for (ulong i = 0; i < 50; i++) {
        p2_exposures.push_back(phase_two_base_exposure_ + exposure_levels->exposure_increment * i);
    }

    auto running = true;
    auto found = false;

    cv::Mat org;

    log_time << "Phase two right begun..\n";

    // ************  RIGHT SIDE ONLY **************

    const std::string win = "what";

    // attempt to find a good exposure for this phase
    while (running) {

        right_frames.clear();

        found = false;

        for (const auto exp : p2_exposures) {

            // adjust exposure and capture frames
            pcapture->exposure(exp);
            pcapture->cap(2, right_frames);

            // configure structures
            org = right_frames.back().clone();
            auto h = org.clone();
            hough_horizontal->original(h);

            // process matrix for line detection
            process_mat_for_line(org, hough_horizontal, pmorph.get());

            // get results
            const auto& lines = hough_horizontal->all_lines(); // inner most side

            //log_time << __FUNCTION__ << " lines.size() : " << lines.size() << '\n';

            // not enough lines, we are greedy
            if (lines.size() < 2) {
                continue;
            }

            // copy found lines to target structure
            for (auto& line : lines) {
                //if (line.entry_[0] > left_cutoff)                
                stl::copy_vector(line.elements_, elements);
            }

            // check if there is enough to work with
            if (elements.size() > 3) { // && elements.front().y != elements.back().y) {
                phase_two_base_exposure_ = exp;
                running = false;
                found = true;
                break;
            }

            // whoops
            if (exp > 100'000) {
                log_err << "left side failed to produce valid lines.";
                return false;
            }

        }

        if (found) {
            log_time << "Phase two _right_ exposure detected.. " << phase_two_base_exposure_ << '\n';
            break;
        }

        log_err << __FUNCTION__ << " unable to detect correct exposure :(\n";
        return false;

    }

    // adjust capture ROI based on found lines.

    auto line_area = cv::minAreaRect(elements);
    auto line_area_rect = line_area.boundingRect();

    log_time << __FUNCTION__ " right boundry detected : " << line_area_rect << '\n';

    auto old_roi = pcapture->region();

    capture_roi new_roi;

    // adjust the new roi according to findings
    new_roi.x = old_roi.x + static_cast<unsigned long>(floor(line_area_rect.x));
    new_roi.y = old_roi.y + static_cast<unsigned long>(floor(line_area_rect.y));
    new_roi.width = static_cast<unsigned long>(ceil(line_area_rect.width));
    new_roi.height = static_cast<unsigned long>(ceil(line_area_rect.height));

    log_time << __FUNCTION__ << " new_roi changed to " << new_roi << '\n';

    // the offset calculation begins with setting the base roi y value
    auto offset_y = static_cast<double>(new_roi.y);
    //auto offset_y = static_cast<double>(phase_roi_[1].y);

    log_time << __FUNCTION__ " init offset_y + new_roi.y : " << offset_y << '\n';

    phase_roi_[1] = new_roi;

    // empty the buffer
    log_time << __FUNCTION__ << " clearing buffer..\n";
    pcapture->region(capture_roi(1, 1, 1, 1));
    pcapture->cap(3, right_frames);
    right_frames.clear();

    // only update region, exposure should be at desired level at this point
    pcapture->region(new_roi);

    // multiply the exposure because it's most likely pretty gloomy there
    pcapture->exposure_mul(DEF_PHASE_TWO_MULTIPLIER);

    running = true;

    auto const frame_count = 25;

    auto right_y = 0.0;

    //cv::namedWindow("morph");

    // capture right frames for real and process the result
    while (running) {

        right_y = 0.0;
        right_frames.clear();
        elements.clear();
        hough_horizontal->clear();

        pcapture->cap(frame_count, right_frames);

        if (right_frames.empty()) {
            log_err << __FUNCTION__ << " fatal error, no frames were captured!\n";
            continue;
        }

        // iterate through the captured frames, don't skip any as the buffer should be alright.
        for (const auto& right : right_frames) {

            org = right.clone();
            auto h = right.clone();
            hough_horizontal->original(h);

            process_mat_for_line(org, hough_horizontal, pmorph.get());

            // grab everything, since we already have defined the roi earlier
            const auto& lines = hough_horizontal->all_lines();

            for (const auto& line : lines) {
                stl::copy_vector(line.elements_, elements);
            }

        }

        if (elements.empty()) {
            log_err << __FUNCTION__ " fatal error, elements are empty!\n";
            return false;
        }

        auto boundry_area = cv::minAreaRect(elements);
        auto boundry_area_rect = boundry_area.boundingRect2f();

        // adjust to reduce crap
        //left_boundry_rect.width -= 40;

        cvr::rect_force_align_boundries<float, 0>(boundry_area_rect, static_cast<float>(right_frames.front().cols), static_cast<float>(right_frames.front().rows));

        if (!validate::validate_rect(boundry_area_rect)) {
            log_err << __FUNCTION__ << " invalid rect in phase two [right], restarting phase two.\n";
            continue;
        }

        auto t = org(boundry_area_rect);

        try {
            right_y += calc::real_intensity_line(t, pdata->right_points, t.rows, 0);
        } catch (cv::Exception& e) {
            log_err << __FUNCTION__ << " " << e.what() << '\n';
            continue;
        }

        running = false;

    }

    offset_y += right_y;

    log_time << __FUNCTION__ " offset_y + right_y : " << offset_y << '\n';

    pdata->base_lines[3] = offset_y;

    // align points the match the real location in the image.
    for (auto& p : pdata->right_points) {
        p.y += offset_y;
        //std::cout << p << " - ";
    }

    std::cout << '\n';

    const std::string ko = "_phase2_right.png";
    // snap the freaking image
    cv::imwrite(ko, right_frames.front());

    log_time << "right baseline: " << pdata->base_lines[3] << '\n';

    // return exposure to "normal"
    pcapture->exposure_div(DEF_PHASE_TWO_MULTIPLIER);

    // update the phase roi for right side
    //phase_roi<int, 1>(line_area_rect);

    return true;

}

bool Seeker::phase_two_line() {

    const double FUTILE_ACCEPTENCE = 0.5;

    log_time << __FUNCTION__ " started..\n";

    switch_phase();

    std::vector<cv::Mat> left_frames;

    // clear any buffer
    pcapture->region(buffer_clear_roi);
    pcapture->cap(3, left_frames);
    left_frames.clear();

    // prep for next phase
    pcapture->region(phase_roi_[1]);

    auto phase = frameset(current_phase_);

    // make sure the minimum is at least 10 pixels.
    auto min_line_len = calc::line::compute_houghp_min_line(10.0, pdata->marking_rect);

    // horizontal houghline extension class
    auto hough_horizontal = make_shared<HoughLinesPR>(1, calc::round(calc::DEGREES), 40, calc::round(min_line_len), false);

    hough_horizontal->max_line_gab(12);

    hough_horizontal->marking_rect(cv::Rect2d(phase_roi_[1].x, phase_roi_[1].y, phase_roi_[1].width, phase_roi_[1].height));

    //auto left_size = cv::Size(left_baseline.width, left_baseline.height);
    auto left_cutoff = phase_roi_[1].width / 2.0;

    return true;
}

bool Seeker::phase_three() {

    // gogo!

    // lower the exposure to 50%
    pcapture->exposure(pcapture->exposure() / 2);

    log_time << __FUNCTION__ << " configuration started.\n";

    capture_roi phase_3_roi;

    phase_3_roi.x = static_cast<ulong>(floor(pdata->marking_rect.x));
    phase_3_roi.y = phase_roi_[0].y;
    phase_3_roi.width = static_cast<ulong>(ceil(pdata->marking_rect.width));
    phase_3_roi.height = phase_roi_[0].height;

    phase_roi_[2] = phase_3_roi;

    auto def_y = phase_3_roi.y;// +phase_3_roi.height;

    std::vector<cv::Mat> frames;
    std::vector<cv::Point2d> results(phase_3_roi.width);
    stl::populate_x(results, phase_3_roi.width);

    // capture 3 frames quickly to empty the buffer
    log_time << __FUNCTION__ << " clearing buffer..\n";
    pcapture->region(buffer_clear_roi);
    pcapture->cap(3, frames);
    frames.clear();

    pcapture->region(phase_3_roi);

    const auto frame_count = 25;

    std::vector<cv::Rect> laser_rects_y;
    laser_rects_y.reserve(frame_count);

    const auto binary_threshold = 100;

    auto running = true;

    auto failures = 0;

    log_time << __FUNCTION__ << " configuration done.. capturing.\n";

    frames.clear();

    pcapture->cap(frame_count, frames);

    while (running) {

        auto avg_height = 0.0;

        stl::reset_point_y(results);

        cv::Rect laser_rect_y;
        laser_rects_y.clear();

        for (auto i = frame_count; i--;) {

            cv::Mat base_frame;

            try { {
                    // Highly experimental, could be improved?
                    // TODO : replace with custom filter if needed
                    cv::bilateralFilter(frames[i], base_frame, 3, 20, 10);
                    threshold(base_frame, base_frame, binary_threshold, 255, CV_THRESH_BINARY);
                    GaussianBlur(base_frame, base_frame, cv::Size(5, 5), 0, 10, cv::BORDER_DEFAULT);
                }

                //cv::imwrite("_laser_" + std::to_string(i) + ".png", base_frame);

                /* RECT CUT METHOD - testing */
                avg_height += calc::weighted_avg(base_frame, base_frame, pdata->center_points, laser_rect_y);

                laser_rects_y.emplace_back(std::move(laser_rect_y));

                //log_time << __FUNCTION__ << " laser rect Y : " << laser_rect_y << '\n';

                throw_assert(validate::valid_pix_vec(pdata->center_points), "Centerpoints failed validation!!!");

                for (auto& centerpoint : pdata->center_points) {
                    results[static_cast<int>(centerpoint.x)].y += centerpoint.y;
                }

            } catch (std::exception& e) {
                log_err << e.what() << std::endl;
                failures++;
            }

        }

        log_time << cv::format("Center point data gathering failures : %i\n", failures);

        pdata->points_start[1] = pdata->center_points.front().x + phase_3_roi.x;

        for (auto& centerpoint : pdata->center_points) {
            results[static_cast<int>(centerpoint.x)].y /= frame_count;
            if (centerpoint.x + pdata->marking_rect.x < pdata->points_start[1]) {
                pdata->points_start[1] = centerpoint.x + pdata->marking_rect.x;
            }
        }

        // since theres some issues with using results vector, this works just as fine.
        pdata->center_points.clear();
        stl::copy_vector(results, pdata->center_points);

        auto avg_laser_rect = calc::avg(laser_rects_y);

        log_time << __FUNCTION__ " laser rectangles avg : " << avg_laser_rect << '\n';

        auto highest_total = avg_height / static_cast<unsigned int>(frame_count);

        log_time << __FUNCTION__ " raw laser avg from contained rect : " << highest_total << '\n';

        // correct the offsets, so the location is correct in the full camera roi
        highest_total += def_phase_one_roi_.y;

        log_time << __FUNCTION__ " + def_phase_one_roi_.y : " << highest_total << '\n';

        highest_total += avg_laser_rect.y;

        log_time << __FUNCTION__ " + avg_laser_rect.y : " << highest_total << '\n';

        //highest_total -= avg_laser_rect.height;

        //log_time << __FUNCTION__ " - avg_laser_rect.height : " << highest_total << '\n';

        avg_height = 0.0;

        log_time << cv::format("pdata->base_lines[1]: %f\n", pdata->base_lines[1]);
        log_time << cv::format("highest_total: %f\n", highest_total);

        pdata->difference = abs(pdata->base_lines[1] - highest_total);
        std::string dif = cv::format("diff from baseline: %f\n", pdata->difference);
        log_err << dif;
        std::cout << dif;

        running = false;

        for (auto& p : pdata->center_points) {
            p.y += avg_laser_rect.y + phase_3_roi.y; // should be correct
            //std::cout << p << " - ";
        }

        //std::cout << '\n';

        const std::string ko = "_phase3_muhko.png";
        // snap the freaking image
        cv::imwrite(ko, frames.front());

    }

    return true;

}

int Seeker::frameset(Phase phase) {
    switch (phase) {
        case Phase::ONE:
            return 0;
        case Phase::TWO_RIGHT:
            return 1;
        case Phase::TWO_LEFT:
            return 2;
        case Phase::THREE:
            return 3;
        default:
            return -1;
    }
}

bool Seeker::compute(bool do_null, cv::Rect_<unsigned long>& marking_rect, unsigned long p2_base_exposure) {

    if (!initialize())
        return false;

    auto phase_complete = false;

    // check if its a null computation, if so apply stuff
    if (do_null) {

        if (!validate::validate_rect(marking_rect)) {
            log_err << __FUNCTION__ << " is unable to perform zero height measurement, rectangle is invalid.\n";
            return false;
        }

        // skip the entirety of phase_one, just feed the marking_rect directly
        pdata->marking_rect = cvr::rect_any_to_double(marking_rect);

        log_time << __FUNCTION__ << " marking rect found : " << pdata->marking_rect << '\n';

        // set phase two roi right away.

        auto quarter = phase_roi_[0].height / 4;
        phase_roi_[1].x = static_cast<unsigned long>(ceil(pdata->marking_rect.x) / 2);
        phase_roi_[1].y = phase_roi_[0].y + 3 * quarter;
        phase_roi_[1].width = phase_roi_[1].x;
        phase_roi_[1].height = quarter;

        phase_two_base_exposure_ = p2_base_exposure;

        log_time << "phase two left region configured : " << phase_roi_[1] << '\n';

    } else {
        phase_complete = phase_one();

        if (!phase_complete) {
            log_err << __FUNCTION__ " phase one FAILED..\n";
            return false;
        }
    }

    log_time << __FUNCTION__ << " phase one completed ok..\n";

    phase_complete = phase_two_left();

    if (!phase_complete) {
        log_err << __FUNCTION__ " phase two left FAILED..\n";
        return false;
    }

    log_time << __FUNCTION__ << " phase two left completed ok..\n";

    phase_complete = phase_three();

    if (!phase_complete) {
        log_err << __FUNCTION__ " phase three FAILED..\n";
    }

    pcapture->aquisition_end();

    pcapture->cap_end();

    pcapture->close();

    pcapture->uninitialize();

    auto near_height = phase_finalize();

    std::cout << "near_height : " << near_height << '\n';
    log_err << "near_height : " << near_height << '\n';

    return phase_complete;
}
