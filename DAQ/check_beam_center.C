#include <TFile.h>
#include <TCanvas.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TH2F.h>
#include <TF1.h>
#include <TLine.h>
#include <TMarker.h>
#include <TLatex.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <iostream>
#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// MALTA2 Beam Center Check
// Usage: check_beam_center("output/beamcheck_runXXX.root", "output/beamcheck/beamcheck_runXXX.png")
//
// Reads from Corryvreckan beamcheck run (MaskCreator module):
//   MaskCreator/DETECTOR/occupancy  (TH2D, pixel coords, hits/px/event)
//   MaskCreator/DETECTOR/maskmap    (TH2F, 1 = masked pixel)
//
// Fits a Gaussian to X and Y projections (2-step: rough → ±2σ refined).
// Beam center = Gaussian mean. Falls back to weighted centroid if fit fails.
// Displays µ, σ, χ²/ndf on each projection pad.
//
// Layout: 3 columns (one per detector) x 3 rows
//   Row 1 : 2D occupancy map
//   Row 2 : X projection (column profile, blue) + Gaussian fit
//   Row 3 : Y projection (row profile,    red)  + Gaussian fit
//
// WARN : |offset| > 1.5 mm
// FAIL : |offset| > 3.0 mm  -> gSystem->Exit(1)
// ---------------------------------------------------------------------------

// Gaussian + flat background fit over the histogram's current axis range.
// Call h->GetXaxis()->SetRange(lo, hi) before this to restrict the fit region.
//
// Model: f(x) = A * exp(-0.5*((x-mu)/sig)^2) + bg
//
// Background is estimated from the edges of the valid range.
// FWHM is computed on the background-subtracted distribution to get a
// robust initial sigma that is not inflated by the flat noise pedestal.
//
// Returns true on success. Also returns fitted amplitude (amp) and
// background (bg) so the caller can draw the full model curve.
bool fitGaus(TH1D* h, double& mean, double& sigma, double& chi2ndf,
             double& amp, double& bg) {
    if (!h || h->GetEntries() < 10) return false;

    // Use the histogram's current axis range (set by SetRange before calling)
    int    lo  = h->GetXaxis()->GetFirst();
    int    hi  = h->GetXaxis()->GetLast();
    if (lo >= hi) return false;

    double x_lo = h->GetBinCenter(lo);
    double x_hi = h->GetBinCenter(hi);

    // --- Background estimate from edge bins of the valid range ---
    int    bg_w = std::min(20, (hi - lo) / 4);
    double bg_sum = 0;
    int    bg_n   = 0;
    for (int b = lo;          b < lo + bg_w; b++) { bg_sum += h->GetBinContent(b); bg_n++; }
    for (int b = hi - bg_w+1; b <= hi;       b++) { bg_sum += h->GetBinContent(b); bg_n++; }
    double bg_est = (bg_n > 0) ? bg_sum / bg_n : 0.0;

    // --- Peak search within valid range ---
    int    peak_bin = lo;
    double peak_val = 0;
    for (int b = lo; b <= hi; b++) {
        if (h->GetBinContent(b) > peak_val) { peak_val = h->GetBinContent(b); peak_bin = b; }
    }
    double peak_x      = h->GetBinCenter(peak_bin);
    double peak_height = peak_val - bg_est;
    if (peak_height <= 0) return false;

    // --- FWHM on background-subtracted values for initial sigma ---
    double half = peak_height / 2.0;
    int    lb   = lo, rb = hi;
    for (int b = peak_bin; b >= lo; b--) {
        if ((h->GetBinContent(b) - bg_est) < half) { lb = b; break; }
    }
    for (int b = peak_bin; b <= hi; b++) {
        if ((h->GetBinContent(b) - bg_est) < half) { rb = b; break; }
    }
    double sig_init = std::max(1.0, (h->GetBinCenter(rb) - h->GetBinCenter(lb)) / 2.355);

    // --- Step 1: rough fit with gaus + pol0 in [peak ± 2σ] ---
    double r1_lo = std::max(x_lo, peak_x - 2*sig_init);
    double r1_hi = std::min(x_hi, peak_x + 2*sig_init);
    TF1 f1("f1_gb", "gaus(0)+pol0(3)", r1_lo, r1_hi);
    f1.SetParameters(peak_height, peak_x, sig_init, bg_est);
    f1.SetParLimits(0, 0, peak_val * 2);   // amplitude > 0
    f1.SetParLimits(2, 0.5, x_hi - x_lo);  // sigma > 0
    int st1 = h->Fit(&f1, "RQBN");  // B=use limits

    double mu1  = (st1 == 0) ? f1.GetParameter(1) : peak_x;
    double sig1 = (st1 == 0) ? std::abs(f1.GetParameter(2)) : sig_init;
    double amp1 = (st1 == 0) ? f1.GetParameter(0) : peak_height;
    double bg1  = (st1 == 0) ? f1.GetParameter(3) : bg_est;
    if (sig1 <= 0) sig1 = sig_init;

    // --- Step 2: refined fit in [mu ± 2σ] ---
    double r2_lo = std::max(x_lo, mu1 - 2*sig1);
    double r2_hi = std::min(x_hi, mu1 + 2*sig1);
    TF1 f2("f2_gb", "gaus(0)+pol0(3)", r2_lo, r2_hi);
    f2.SetParameters(amp1, mu1, sig1, bg1);
    f2.SetParLimits(0, 0, peak_val * 2);
    f2.SetParLimits(2, 0.5, x_hi - x_lo);
    int st2 = h->Fit(&f2, "RQBN");

    if (st2 != 0) return false;

    mean    = f2.GetParameter(1);
    sigma   = std::abs(f2.GetParameter(2));
    amp     = f2.GetParameter(0);
    bg      = f2.GetParameter(3);
    chi2ndf = (f2.GetNDF() > 0) ? f2.GetChisquare() / f2.GetNDF() : -1;
    return true;
}
void check_beam_center(TString filename, TString output_png) {

    const double WARN_MM  = 1.5;
    const double FAIL_MM  = 3.0;
    const double PITCH_MM = 0.0364;   // 36.4 um/pixel

    // MALTA2 geometry
    const int    N_COL   = 224;
    const int    N_ROW   = 512;
    const double CTR_COL = (N_COL - 1) / 2.0;   // 111.5
    const double CTR_ROW = (N_ROW - 1) / 2.0;   // 255.5

    // Extract run number  (e.g. "output/beamcheck_run001.root" -> "001")
    TString run_str = filename;
    run_str.Remove(0, run_str.Last('/') + 1);
    run_str.ReplaceAll("beamcheck_run", "");
    run_str.ReplaceAll(".root", "");

    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetPalette(kBird);
    gStyle->SetNumberContours(99);

    TFile* f = TFile::Open(filename);
    if (!f || f->IsZombie()) {
        std::cout << "[BEAM_CHECK][ERROR] Cannot open: " << filename << std::endl;
        gSystem->Exit(1);
        return;
    }

    std::vector<TString> detectors = {"MALTA_0", "MALTA_1", "MALTA_2"};
    bool any_fail = false;

    // -------------------------------------------------------------------------
    // Canvas  (3 columns x 3 rows + title strip)
    // -------------------------------------------------------------------------
    TCanvas* c = new TCanvas("beam_check", Form("Beam Center Check  Run %s", run_str.Data()), 1800, 1500);

    // Title strip
    TPad* p_title = new TPad("p_title", "", 0.0, 0.955, 1.0, 1.0);
    p_title->SetFillColor(kGray + 3);
    p_title->SetBorderMode(0);
    p_title->Draw();
    p_title->cd();
    TLatex title_tex;
    title_tex.SetNDC();
    title_tex.SetTextFont(62);
    title_tex.SetTextColor(kWhite);
    title_tex.SetTextSize(0.52);
    title_tex.DrawLatex(0.02, 0.20,
        Form("MALTA2 Beam Center Check   Run %s     |  WARN > %.1f mm   FAIL > %.1f mm",
             run_str.Data(), WARN_MM, FAIL_MM));
    c->cd();

    // Row label strip on the left (narrow vertical pad)
    TPad* p_labels = new TPad("p_labels", "", 0.0, 0.0, 0.022, 0.953);
    p_labels->SetFillColor(kGray + 2);
    p_labels->SetBorderMode(0);
    p_labels->Draw();
    p_labels->cd();
    TLatex rl;
    rl.SetNDC(); rl.SetTextFont(62); rl.SetTextColor(kWhite); rl.SetTextAngle(90);
    rl.SetTextSize(0.055); rl.DrawLatex(0.22, 0.79,  "2D Occupancy");
    rl.SetTextSize(0.050); rl.DrawLatex(0.22, 0.475, "X Projection (Column)");
    rl.SetTextSize(0.050); rl.DrawLatex(0.22, 0.145, "Y Projection (Row)");
    c->cd();

    // Pad grid: [row][col]  row=0:2D  row=1:Xproj  row=2:Yproj
    // x ranges from 0.022 (after label strip) to 1.0, split into 3 columns
    // y ranges (bottom=0, top=1):
    //   2D:    [0.640, 0.953]
    //   Xproj: [0.330, 0.637]
    //   Yproj: [0.010, 0.327]
    const double y_top[3]    = {0.953, 0.637, 0.327};
    const double y_bot[3]    = {0.640, 0.330, 0.010};
    const double x_left_base = 0.022;

    TPad* pads_2d[3];
    TPad* pads_px[3];
    TPad* pads_py[3];

    for (int i = 0; i < 3; i++) {
        double xl = x_left_base + (1.0 - x_left_base) * i / 3.0;
        double xr = x_left_base + (1.0 - x_left_base) * (i + 1) / 3.0;

        pads_2d[i] = new TPad(Form("pad2d_%d", i), "", xl, y_bot[0], xr, y_top[0]);
        pads_2d[i]->SetRightMargin(0.15);
        pads_2d[i]->SetLeftMargin(0.13);
        pads_2d[i]->SetTopMargin(0.12);
        pads_2d[i]->SetBottomMargin(0.10);
        pads_2d[i]->Draw();

        pads_px[i] = new TPad(Form("padpx_%d", i), "", xl, y_bot[1], xr, y_top[1]);
        pads_px[i]->SetRightMargin(0.05);
        pads_px[i]->SetLeftMargin(0.13);
        pads_px[i]->SetTopMargin(0.10);
        pads_px[i]->SetBottomMargin(0.14);
        pads_px[i]->Draw();

        pads_py[i] = new TPad(Form("padpy_%d", i), "", xl, y_bot[2], xr, y_top[2]);
        pads_py[i]->SetRightMargin(0.05);
        pads_py[i]->SetLeftMargin(0.13);
        pads_py[i]->SetTopMargin(0.10);
        pads_py[i]->SetBottomMargin(0.14);
        pads_py[i]->Draw();
    }

    // -------------------------------------------------------------------------
    std::cout << "\n================================================================" << std::endl;
    std::cout << "  BEAM CENTER CHECK  --  Run " << run_str << std::endl;
    std::cout << "  MALTA2 : " << N_COL << " x " << N_ROW
              << " px  @  " << PITCH_MM*1000 << " um/px" << std::endl;
    std::cout << "  Noise-filtered centroid (maskmap pixels excluded)" << std::endl;
    std::cout << "  WARN : |offset| > " << WARN_MM << " mm" << std::endl;
    std::cout << "  FAIL : |offset| > " << FAIL_MM << " mm" << std::endl;
    std::cout << "================================================================" << std::endl;

    // -------------------------------------------------------------------------
    for (int i = 0; i < 3; i++) {
        TString det       = detectors[i];
        TString occ_path  = Form("MaskCreator/%s/occupancy", det.Data());
        TString mask_path = Form("MaskCreator/%s/maskmap",   det.Data());

        TH2D* h_occ  = (TH2D*)f->Get(occ_path);
        TH2F* h_mask = (TH2F*)f->Get(mask_path);

        if (!h_occ) {
            pads_2d[i]->cd();
            TLatex missing;
            missing.SetNDC(); missing.SetTextSize(0.06); missing.SetTextColor(kRed);
            missing.DrawLatex(0.1, 0.5, Form("%s: occupancy not found", det.Data()));
            std::cout << "  [SKIP]  " << det << " : not found at " << occ_path << std::endl;
            continue;
        }

        // ------------------------------------------------------------------
        // Noise-filtered centroid: skip pixels flagged in maskmap
        // ------------------------------------------------------------------
        double sum_w  = 0, sum_wx = 0, sum_wy = 0;
        int    n_masked = 0;

        for (int col = 1; col <= h_occ->GetNbinsX(); col++) {
            for (int row = 1; row <= h_occ->GetNbinsY(); row++) {
                if (h_mask && h_mask->GetBinContent(col, row) > 0) {
                    n_masked++;
                    continue;
                }
                double occ = h_occ->GetBinContent(col, row);
                if (occ <= 0) continue;
                double x = h_occ->GetXaxis()->GetBinCenter(col);
                double y = h_occ->GetYaxis()->GetBinCenter(row);
                sum_w  += occ;
                sum_wx += occ * x;
                sum_wy += occ * y;
            }
        }

        double centroid_col = (sum_w > 0) ? sum_wx / sum_w : CTR_COL;
        double centroid_row = (sum_w > 0) ? sum_wy / sum_w : CTR_ROW;
        double entries      = h_occ->GetEntries();

        // Gaussian fit results (filled later after projections are made)
        double gaus_col = centroid_col, gaus_sig_col = 0, gaus_chi2_col = -1;
        double gaus_row = centroid_row, gaus_sig_row = 0, gaus_chi2_row = -1;
        double gaus_amp_col = 0, gaus_bg_col = 0;
        double gaus_amp_row = 0, gaus_bg_row = 0;
        bool   fit_ok_col = false, fit_ok_row = false;

        // Primary beam center: Gaussian mean (fallback: weighted centroid)
        double mean_col = centroid_col;
        double mean_row = centroid_row;
        double off_col  = (mean_col - CTR_COL) * PITCH_MM;
        double off_row  = (mean_row - CTR_ROW) * PITCH_MM;

        const char* status;
        int          status_color;
        if (std::abs(off_col) > FAIL_MM || std::abs(off_row) > FAIL_MM) {
            status       = "FAIL";
            status_color = kRed;
            any_fail     = true;
        } else if (std::abs(off_col) > WARN_MM || std::abs(off_row) > WARN_MM) {
            status       = "WARN";
            status_color = kOrange + 1;
        } else {
            status       = " OK ";
            status_color = kGreen + 2;
        }

        // ------------------------------------------------------------------
        // Build display histogram: zero out masked pixels
        // ------------------------------------------------------------------
        TH2D* h_disp = (TH2D*)h_occ->Clone(Form("h_disp_%d", i));
        h_disp->SetDirectory(0);
        if (h_mask) {
            for (int col = 1; col <= h_disp->GetNbinsX(); col++)
                for (int row = 1; row <= h_disp->GetNbinsY(); row++)
                    if (h_mask->GetBinContent(col, row) > 0)
                        h_disp->SetBinContent(col, row, 0);
        }

        // ==================================================================
        // Row 0 : 2D occupancy map
        // ==================================================================
        pads_2d[i]->cd();

        h_disp->GetXaxis()->SetTitle("Column [px]");
        h_disp->GetYaxis()->SetTitle("Row [px]");
        h_disp->GetXaxis()->SetTitleSize(0.055);
        h_disp->GetYaxis()->SetTitleSize(0.055);
        h_disp->GetXaxis()->SetLabelSize(0.048);
        h_disp->GetYaxis()->SetLabelSize(0.048);
        h_disp->Draw("colz");

        double xmin = h_disp->GetXaxis()->GetXmin();
        double xmax = h_disp->GetXaxis()->GetXmax();
        double ymin = h_disp->GetYaxis()->GetXmin();
        double ymax = h_disp->GetYaxis()->GetXmax();

        // Sensor-centre crosshair (dashed white)
        TLine* lh = new TLine(xmin, CTR_ROW, xmax, CTR_ROW);
        lh->SetLineColor(kWhite); lh->SetLineWidth(1); lh->SetLineStyle(2); lh->Draw("same");
        TLine* lv = new TLine(CTR_COL, ymin, CTR_COL, ymax);
        lv->SetLineColor(kWhite); lv->SetLineWidth(1); lv->SetLineStyle(2); lv->Draw("same");

        // Initial centroid marker (updated after Gaussian fit at end of loop)
        TMarker* mk = new TMarker(centroid_col, centroid_row, 29);
        mk->SetMarkerColor(kGray + 1);
        mk->SetMarkerSize(2.0);
        mk->Draw("same");

        // Static annotations (status/color updated after fit)
        TLatex lat;
        lat.SetNDC(); lat.SetTextFont(62); lat.SetTextSize(0.063);
        lat.SetTextColor(kBlack); lat.DrawLatex(0.17, 0.88, Form("%s", det.Data()));
        lat.SetTextColor(status_color); lat.DrawLatex(0.60, 0.88, Form("[%s]", status));
        lat.SetTextFont(42); lat.SetTextSize(0.050); lat.SetTextColor(kBlack);
        lat.DrawLatex(0.17, 0.67, Form("masked = %d px", n_masked));

        // ==================================================================
        // Row 1 : X projection (column, summed over rows) + Gaussian fit
        // ==================================================================
        pads_px[i]->cd();

        // Project X (column profile), summing only rows 11..N_ROW-10
        // then restrict the column axis to 11..N_COL-10 for display and fit
        const int EDGE = 10;
        TH1D* h_px = h_disp->ProjectionX(Form("h_px_%d", i), EDGE+1, N_ROW-EDGE);
        h_px->GetXaxis()->SetRange(EDGE+1, N_COL-EDGE);
        h_px->SetDirectory(0);
        h_px->SetLineColor(kBlue + 1);
        h_px->SetLineWidth(2);
        h_px->SetFillColorAlpha(kBlue + 1, 0.25);
        h_px->GetXaxis()->SetTitle("Column [px]");
        h_px->GetYaxis()->SetTitle("Hits");
        h_px->GetXaxis()->SetTitleSize(0.058);
        h_px->GetYaxis()->SetTitleSize(0.058);
        h_px->GetXaxis()->SetLabelSize(0.050);
        h_px->GetYaxis()->SetLabelSize(0.050);
        h_px->GetYaxis()->SetTitleOffset(1.0);
        h_px->Draw("HIST");

        // Gaussian + background fit
        fit_ok_col = fitGaus(h_px, gaus_col, gaus_sig_col, gaus_chi2_col, gaus_amp_col, gaus_bg_col);
        if (fit_ok_col) {
            mean_col = gaus_col;
            off_col  = (mean_col - CTR_COL) * PITCH_MM;
            TF1* fgx = new TF1(Form("fgx_%d", i), "gaus(0)+pol0(3)",
                               gaus_col - 3*gaus_sig_col, gaus_col + 3*gaus_sig_col);
            fgx->SetParameters(gaus_amp_col, gaus_col, gaus_sig_col, gaus_bg_col);
            fgx->SetLineColor(kAzure - 2);
            fgx->SetLineWidth(3);
            fgx->Draw("same");
        }

        double px_max = h_px->GetMaximum() * 1.35;
        h_px->SetMaximum(px_max);

        // Sensor centre (dashed gray)
        TLine* lx_ctr = new TLine(CTR_COL, 0, CTR_COL, px_max);
        lx_ctr->SetLineColor(kGray + 1); lx_ctr->SetLineWidth(1); lx_ctr->SetLineStyle(2);
        lx_ctr->Draw("same");

        // Beam center (solid, status colour)
        TLine* lx_mean = new TLine(mean_col, 0, mean_col, px_max);
        lx_mean->SetLineColor(status_color); lx_mean->SetLineWidth(2);
        lx_mean->Draw("same");

        TLatex latpx;
        latpx.SetNDC(); latpx.SetTextFont(62); latpx.SetTextSize(0.055);
        latpx.SetTextColor(kBlack);
        latpx.DrawLatex(0.17, 0.89, Form("%s  —  X Projection", det.Data()));
        latpx.SetTextFont(42); latpx.SetTextSize(0.050);
        latpx.SetTextColor(status_color);
        latpx.DrawLatex(0.17, 0.80, Form("dX = %+.2f mm", off_col));
        if (fit_ok_col) {
            latpx.SetTextColor(kAzure - 2);
            latpx.DrawLatex(0.17, 0.71, Form("#mu = %.1f px  (#pm%.1f px)", gaus_col, gaus_sig_col));
            latpx.DrawLatex(0.17, 0.62, Form("#sigma = %.1f px  (%.0f #mum)", gaus_sig_col, gaus_sig_col * PITCH_MM * 1000));
            latpx.SetTextColor(kGray + 1);
            latpx.DrawLatex(0.17, 0.53, Form("#chi^{2}/ndf = %.2f", gaus_chi2_col));
        } else {
            latpx.SetTextColor(kGray + 1);
            latpx.DrawLatex(0.17, 0.71, Form("centroid = %.1f px (fit failed)", centroid_col));
        }

        // ==================================================================
        // Row 2 : Y projection (row, summed over columns) + Gaussian fit
        // ==================================================================
        pads_py[i]->cd();

        // Project Y (row profile), summing only columns 11..N_COL-10
        // then restrict the row axis to 11..N_ROW-10 for display and fit
        TH1D* h_py = h_disp->ProjectionY(Form("h_py_%d", i), EDGE+1, N_COL-EDGE);
        h_py->GetXaxis()->SetRange(EDGE+1, N_ROW-EDGE);
        h_py->SetDirectory(0);
        h_py->SetLineColor(kRed + 1);
        h_py->SetLineWidth(2);
        h_py->SetFillColorAlpha(kRed + 1, 0.25);
        h_py->GetXaxis()->SetTitle("Row [px]");
        h_py->GetYaxis()->SetTitle("Hits");
        h_py->GetXaxis()->SetTitleSize(0.058);
        h_py->GetYaxis()->SetTitleSize(0.058);
        h_py->GetXaxis()->SetLabelSize(0.050);
        h_py->GetYaxis()->SetLabelSize(0.050);
        h_py->GetYaxis()->SetTitleOffset(1.0);
        h_py->Draw("HIST");

        // Gaussian + background fit
        fit_ok_row = fitGaus(h_py, gaus_row, gaus_sig_row, gaus_chi2_row, gaus_amp_row, gaus_bg_row);
        if (fit_ok_row) {
            mean_row = gaus_row;
            off_row  = (mean_row - CTR_ROW) * PITCH_MM;
            TF1* fgy = new TF1(Form("fgy_%d", i), "gaus(0)+pol0(3)",
                               gaus_row - 3*gaus_sig_row, gaus_row + 3*gaus_sig_row);
            fgy->SetParameters(gaus_amp_row, gaus_row, gaus_sig_row, gaus_bg_row);
            fgy->SetLineColor(kOrange + 7);
            fgy->SetLineWidth(3);
            fgy->Draw("same");
        }

        double py_max = h_py->GetMaximum() * 1.35;
        h_py->SetMaximum(py_max);

        // Update PASS/FAIL with Gaussian-based offsets
        if (std::abs(off_col) > FAIL_MM || std::abs(off_row) > FAIL_MM) {
            status       = "FAIL";
            status_color = kRed;
            any_fail     = true;
        } else if (std::abs(off_col) > WARN_MM || std::abs(off_row) > WARN_MM) {
            status       = "WARN";
            status_color = kOrange + 1;
        } else {
            status       = " OK ";
            status_color = kGreen + 2;
        }

        // Sensor centre (dashed gray)
        TLine* ly_ctr = new TLine(CTR_ROW, 0, CTR_ROW, py_max);
        ly_ctr->SetLineColor(kGray + 1); ly_ctr->SetLineWidth(1); ly_ctr->SetLineStyle(2);
        ly_ctr->Draw("same");

        // Beam center (solid, status colour)
        TLine* ly_mean = new TLine(mean_row, 0, mean_row, py_max);
        ly_mean->SetLineColor(status_color); ly_mean->SetLineWidth(2);
        ly_mean->Draw("same");

        TLatex latpy;
        latpy.SetNDC(); latpy.SetTextFont(62); latpy.SetTextSize(0.055);
        latpy.SetTextColor(kBlack);
        latpy.DrawLatex(0.17, 0.89, Form("%s  —  Y Projection", det.Data()));
        latpy.SetTextFont(42); latpy.SetTextSize(0.050);
        latpy.SetTextColor(status_color);
        latpy.DrawLatex(0.17, 0.80, Form("dY = %+.2f mm", off_row));
        if (fit_ok_row) {
            latpy.SetTextColor(kOrange + 7);
            latpy.DrawLatex(0.17, 0.71, Form("#mu = %.1f px  (#pm%.1f px)", gaus_row, gaus_sig_row));
            latpy.DrawLatex(0.17, 0.62, Form("#sigma = %.1f px  (%.0f #mum)", gaus_sig_row, gaus_sig_row * PITCH_MM * 1000));
            latpy.SetTextColor(kGray + 1);
            latpy.DrawLatex(0.17, 0.53, Form("#chi^{2}/ndf = %.2f", gaus_chi2_row));
        } else {
            latpy.SetTextColor(kGray + 1);
            latpy.DrawLatex(0.17, 0.71, Form("centroid = %.1f px (fit failed)", centroid_row));
        }

        // Print final result after both fits are done
        std::cout << Form("  [%s]  %-8s  dX=%+.2f mm  dY=%+.2f mm  "
                          "(N=%.0f hits, %d masked px)  "
                          "[sigX=%.1f px, sigY=%.1f px]",
                          status, det.Data(), off_col, off_row,
                          entries, n_masked,
                          gaus_sig_col, gaus_sig_row) << std::endl;

        // Update 2D pad marker and annotations with final Gaussian-based center
        pads_2d[i]->cd();
        TMarker* mk2 = new TMarker(mean_col, mean_row, 29);
        mk2->SetMarkerColor(status_color);
        mk2->SetMarkerSize(3.0);
        mk2->Draw("same");
        TLatex lat2;
        lat2.SetNDC(); lat2.SetTextFont(42); lat2.SetTextSize(0.054);
        lat2.SetTextColor(status_color);
        lat2.DrawLatex(0.17, 0.81, Form("dX = %+.2f mm", off_col));
        lat2.DrawLatex(0.17, 0.74, Form("dY = %+.2f mm", off_row));
    }

    std::cout << "================================================================" << std::endl;

    c->SaveAs(output_png);
    std::cout << "  Saved : " << output_png << std::endl;
    std::cout << "================================================================\n" << std::endl;

    f->Close();

    if (any_fail) {
        std::cout << "  RESULT : BEAM_CHECK_FAIL" << std::endl;
        gSystem->Exit(1);
    } else {
        std::cout << "  RESULT : BEAM_CHECK_PASS" << std::endl;
    }
}
